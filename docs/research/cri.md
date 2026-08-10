### CRI one-function-away pass (cftcoladj, lsc Matching; sfd.h SFSEE_WORK pad fixed; 2026-09-10)
- HAZARD (struct headers): `SFSEE_SHDR` had grown to 0x198 bytes (wave 9) while `SFSEE_WORK`'s
  following pad still assumed 0x30 (`pad8d0[0xAD0 - 0x8D0]`), so every member from `a1hdr` on was
  0x168 too far (sfd_ply `sfply_ResetHn` 0xF38 vs 0xDD0, sfd_see 2/9). Fixing the pad moved sfd_see
  2 -> 8/9, sfd_ply 20 -> 21/22, sfd_adxt 20 -> 22/28 with no regression. After changing a nested
  struct, re-check the parent's offsets: compile a probe `char off_x[(int)&((T *)0)->x];` /
  `char sz[sizeof(T)];` with the CRI flags and read the symbol sizes with objdump -t.
- A multiply of the loop counter by an invariant (`cbtbl[i] = i * v / j`) is strength-reduced by MWCC
  into a decremented IV; writing that IV yourself (`f = j * v; ...; f -= v`) turns it into a source
  local and re-ranks the volatile registers of the block (j/v/f r9/r10/r8 vs r8/r9/r10). A ramp loop
  whose start is another variable's value (`mr r12,r10` copy then the counter continues in r10) is
  `i = v; for (; v <= 0xFF; v++)`, not `for (i = v; ...)` (cftcoladj).
- Temporaries "in place" (`slwi r4,r4,5` then `addi r31,r4,0x38`): read the raw previous id first
  (`id = tbl[(wr_idx + N - 1) % N].id`), then `ent = &tbl[wr_idx]`, then the wrap ternary; the
  inlined-helper order (ent first, id helper second) gave the two temporaries swapped (lsc).
- Two products sharing one load (`rd_nbyte = rqsct * sctlen; rd_ofst = pos * sctlen`): the target's
  load order (`sctlen` first) comes from the *other* statement order (rd_ofst first); a cached local
  changes the registers instead (mfci).
- Bit-reader last read: `cur |= nxt >> k; val = cur >> n;` as two statements gives `or r7,r7,r0` into
  the dead `cur`; `(cur | (nxt >> k)) >> n` computes into r0 (mps_dec BS_GET_LAST).
- Inlined helper returning `if (v >= 0) return v; return -1;` gives the value in r3 and the -1 in r0
  (`li r0,-1 ... mr r0,r3`); `endpos = -1; if (v >= 0) endpos = v; return endpos;` gives a plain
  temp (sfd_see `sfsee_GetInputEndPos`).
- `p = (Uint8 *)(i + (Sint32)buf)` gives `add rP, rI, rBuf` (source operand order) without moving
  the callee-saved ranking; a *second* integer-cast use of `i` in the function does move it
  (adx_bwav: i r18 -> r23). `((buf + i) + 4)` always folds into `addi; lwzx`; no form keeps
  `add r4,r18,r20; lwz r5,4(r4)` (OPEN, adx_bwav `ADX_DecodeInfoWav`).
- Named `static const Float64` literals pool exactly like anonymous ones (M2 confirmed on named
  objects; strip_unused drops non-static globals not in sym_map). Declaring the hoisted loop
  product as a local (`w = (PI/8) * (Float64)i`) fixes the f27/f28 order under the pool (dct_ac).
- Block-local `SFUO_CH *ch = &uo->ch[i]` inside the loop reproduces the target's `addi r6` / store
  interleave (sfd_uo `SFUO_Create` 81 -> 94%); the remaining `mr` copy of `uo` (target steps `uo`
  itself) and `i = 0` copied from the NULL register are M1.
- Unroller compiler-build difference (candidate M6): in an 8x unrolled `(x >> 8) | (x << 8)` on
  `Uint16` the original emits `extrwi 8,16` for the *last* copy's out0 and `srawi 8` for the other
  15; ours emits `srawi` for all 16 (adx_bau `ADXB_ExecOneAu16`, adx_baif `ADXB_ExecOneAiff16`,
  1 instruction each; masked/temp/pointer/operand-order spellings all change every copy).
- Derived-IV placement (adx_stmc `adxstmf_create`): `stm = &obj[ofst++]` gives `mulli` +
  per-iteration `add` with the `addi 0x60` before the `lbz`; the original has it after the `beq`
  (latch block). `ofst++` as a statement / `ofst + i` / for-increment forms turn the pointer itself
  into the IV instead. OPEN (1 instruction).
- adx_baif `AIFF_GetInfo` (80 -> 83%): the FORM/size words share the loop's `ckid`/`cksz` registers
  (`mr r27,r30` / `mr r28,r12` before their last byte insert) and the size is swapped before the
  FORM/AIFF checks; reusing `ckid`/`cksz` for the header removes the loop's `mr` but not the header
  ones (OPEN).
- Still M1 after this pass (no source form moves them): mfci `mfCiReqRd` (mfci r29 vs buf r28),
  mps_dec `mpsdec_DecPackHd` (p never gets adr's r4; nxt/pos/cur/p declaration order is closest),
  mwsfdply `MWSFPLY_SetFlowLimit` (r5..r7 vs r4..r6), rna_res `RNARES_Init` (IV ranked first),
  sfd_set `SFD_SetCond` (hoisted id*4 takes sfd's freed r28 in the target), sfd_lib `SFD_Init`
  (a two-word struct copy gives the registers but loads word 4 before word 0), sfx_alp, sfd_ply
  `SFD_Destroy`, sfx_cnv `SFX_MakeTable` (fp conversion temps/stack slots), mpv_cdec
  `MPVCDEC_IntraBlocks` (the 192-store clear switches to an `mpv+0x720` base at store 84; every
  loop/helper shape keeps r3 then the r31 copy), dct_ac `DCT_AcInit` (M2).

### CRI pass 2 (header-drift probe clean; adx_dcd 7 -> 8/10, mpv_umc 8 -> 11/16, sfd_pts 3 -> 4/5, sfd_tim 29 -> 30/39; 2026-09-10)
- Header-drift probe: a generated `sizeof`/`offsetof` probe over every typedef'd struct of
  src/lib/cri/*.h (89 structs, symbol sizes read with objdump -t) agrees with every `/* 0xNN */`
  comment (the flagged ones are parent-relative comments: SFHDS_AUD/VID, SFSEE_HN), and a
  register-agnostic scan of the target displacements of every unmatched CRI function (multiset of
  `(mnemonic, disp)` per function, target minus ours) finds no consistent delta: the remaining
  displacement differences are pointer-vs-folded shapes (sfd_mpv `addi 0x180/0xd64/0xf80` + `stw 0..`
  = a member pointer kept in a register; sfd_buf `+0x1318` = the SFBUF_HN view; mpv_cdec `-0x38` =
  the documented base switch; cftyp422_ppc = `.bss` object order) or misaligned diffs. No struct pad
  was wrong after the SFSEE_WORK fix. Probe scripts: /tmp/cri2/probe_gen.py, dispscan.py, dispset.py.
- symbols.txt scope: `ADX_DecodeInfoAinf` was `scope:local` while adx_bsc calls it, so dtk emitted
  it as `ADX_DecodeInfoAinf_800BB6F8` and objdiff never paired it (0% in report.json although the
  code was 99% there). A `local` function referenced from another unit must be `scope:global` in
  config/G4BE08/symbols.txt (check `.fn NAME_8xxxxxxx, global` in build/G4BE08/asm/lib/*.s).
- Semantic error found by register tracing (sfd_tim `sftim_IsGetFrmTime`): the target stores and
  compares `ft` (frame time) in `tim->x2c0/x2c8`, our source had `ct`; only the FPR numbers showed
  it (`stfs f2` where f2 = ft). Check which value a `stfs`/`fcmpu` uses before assuming M1.
- Leading integer constant in an add chain with a call operand keeps the chain in source order:
  `(0x1B + infolen + strlen(s) + ofst + align) / align * align` gives `add infolen,r3; add ofst;
  add align; addi 0x1b` (adx_dcd `ADX_CalcHdrInfoLen`, Matching); without the constant MWCC defers
  the leaf adjacent to the call to the end (`infolen + strlen(s) + ofst + align` -> strlen+ofst,
  +align, +infolen) and any statement split puts the sum in the variable's register in place.
- `ofs = 0x14; if (ver == 4) ofs = 0x20;` defines the variable in its callee-saved register
  (`li r28, 0x14; bne; li r28, 0x20`) where `ofs = (ver == 4) ? 0x20 : 0x14;` computes a temporary
  and lets the following `ofs += 4` define it (adx_dcd `ADX_DecodeInfoAinf` 99.1 -> 99.4).
- Inline-asm `li rD, imm` is hoisted by the inline assembler above the preceding independent
  instruction whatever the source order (`addi r4,r4,4; li r5,6` and the reverse both give `li;
  addi`; `#pragma scheduling off` does not change it); `addi r5, 0, 6` keeps its place
  (mpv_umc `mpvumc_OutputIntra6blk`, Matching).
- `mullw` operand order: `mpv->mb_y * 8 * out->cpitch` inline puts the loaded halfword first
  (`mullw cpitch, y8`); a named local on the left (`y8 = mpv->mb_y * 8; ... y8 * out->cpitch`)
  gives `mullw y8, cpitch`; the second offset must derive from it (`y16 = y8 * 2`) so the shift is
  not recomputed from mb_y (mpv_umc `MPVUMC_PpicSkipped`, Matching; `MPVUMC_Intra` 75 -> 84%, its
  `y16` is `mb_y * 16` and the rest is the `mr r5,r3` mpv copy + the late `addi r3,0x380`).
- Volatile temporaries by declaration order fixed sfd_pts `SFPTS_WritePtsQue` (`wr` before `ent`)
  and mpv_umc `MPVUMC_InitOutRfb` (`yh` declared last); brute-forcing all permutations of the
  locals is cheap (`itertools.permutations` + fm.sh, ~0.15 s per build) and worth doing before
  calling a register-only residue M1.
- `volatile Sint32 ext_last` in SFTIM_WORK reproduces the reload after the `!= SFTIM_NONE` test
  (sfd_tim `sftim_GetTimeExtClock`, Matching; no other unit reads it). NOT `vcnt`: the target
  reloads `tim->vcnt` in `SFTIM_IsGetFrmTime` but keeps it in r0 in `SFTIM_IsGetFrmTimeTunit`
  (same inlined helper), so that reload is register pressure, not volatile.
- Shifted-view base for a buffer indexed by a parameter: sfd_pts `SFPTS_ReadPtsQue` through
  `SFBUF_HN *hn = (SFBUF_HN *)((Uint8 *)sfd + strm * sizeof(SFBUF_WORK))` gives the single
  `add r7 = sfd + strm*0x74` base of the target (88 -> 95%); `sfd->buf[strm].u.ring...` recomputes
  the base after the inlined search.
- sfd_tim float pool order (`SFTIM_InitHn` 99.96%): the target's per-function pool is
  [double 0x43300000_80000000][10000.0f][-1.0f] (a 4-byte zero pad before the double), ours
  [10000.0f][double][-1.0f]; one-expression conversions and declaration order do not move it (M2).
- Still M1 after this pass (register-only residue, forms tried): adx_sjd `adxsjd_decode_prep`
  (`ck.len` compare in r5), `adxsjd_get_wr`, `adxsjd_decexec_start`; sfd_cre `sfcre_AnalyMpv`
  (ofs r6 / b4 r4: 180 statement orders), `sfcre_AnalyAudio/Mps`; cri_cvfs `cvFsAddDev`
  (devname r29 / vtbl r28, `mr r0,r3` bounce and the `beq add; b check` after the inlined search;
  helper decl order `dev, i, len` fixed its r26/r27), `cvFsOpen`, `cvFsGetFileSize`; mpv_frm
  `MPV_SkipFrmSj` (mpv r31 / code r30 / sj r29: local copies, `register`, code init tried);
  sfd_tst `SFTST_Calc` (the 64-bit abs diamond: `adiff = diff; if (diff < 0) adiff = -diff;` keeps
  it in place with two `mr`, if/else and ternary forms get sunk to the compare), `SFTST_Create`
  (the `lwz sftst_debout_buf` scheduled above the hdr copy tail); sfd_pts `SFPTS_ReadPtsQue`
  (ofst/size in r30/r29 of the target, volatile in ours); sfx_zmv (inlined helper src/dst r3/r4);
  mwsfdsvr `mwlSfdSleepDecSvr` (two zero copies `mr r30,r28; mr r31,r28` for the inlined
  ClrSleepBdr's two stores; FALSE/0U/local/SetSleepBdr(0) forms all give one `li`),
  `mwSfdExecDecSvrHndl` (rodata base `lis r4` above the prologue stores); sfd_tim
  `sftim_Tc2Time*` (the running sum in the freed r4), `SFTIM_IsStagnant` (else-arm load order),
  `SFTIM_IsGetFrmTime*`; mpv_umc `MPVUMC_Forward/Backward` (`&mpv->mcwk` kept in r30 across the
  call, documented OPEN), `MPVUMC_BiDirect`; sfd_buf `SFBUF_RingGetDataSiz` (`mr r5,r4` zero copy;
  `len1 = len2 = 0`, `len2 = len1` give `li`); mwsfdcre `mwsfcre_MallocRfb` (`blt fail; bge ok`
  after the `||`: helper/goto/`&&`/else-if forms tried). M4 (stwbrx): all 7 sfh_main `SFH_Anly*`.
  M2: adx_dcd `ADX_GetCoefficient`, sfd_adxt `SFADXT_SetSpeed`. M5/M1: mpvabdec (untouched).
- sfd_buf `SFBUF_InitHn`: the inlined `sfbuf_InitAout` clears `u.aout.rsv[0..6]` and then
  `u.ring.dlm_pos/dlm_len/wtot`, which alias the first three words through the union, so ours
  stores `0x1f8/0x1fc/0x200` twice (buffer 4) and `0x2e0..0x2e8` twice (buffer 6) where the target
  has six distinct stores `0x1f8..0x20c` / `0x2e0..0x2f4`: a source-side field set error, not
  header drift (not fixed this pass).

### CRI pass 3 (adx_bwav, sfd_lib, sfd_uo Matching; mwsfdsfx 24 -> 27/28 functions, sfd_see 26 -> 24 words; 2026-09-10)
Harness: ~/.cache/cri3/ (`bytecmp.py lib/<unit>` = per-section byte identity of the split
object vs ours with relocation fields masked and dtk's alignment pad tolerated — the only judge to
use before flipping, objdiff hides .text *order* changes because it pairs by symbol; `mwcc.sh src.c
out.o` = standalone CRI-flag compile of a probe file; `triage.py`, `fm.sh`, `perm_bau.py`).
- MWCC inline asm on `register` locals is the workable "asm for one residual" form: operands are
  variable names (`asm { lwz p1, 4(prm) }`, `asm { addi o, o, 0x60 }`), every variable named in an
  asm block (parameters too) must be `register`, the registers are still the allocator's, and the
  inline assembler is NOT opaque: a following C use folds an asm `addi base, hi, sym@l` into `lwzu
  sym@l(hi)`, and an asm statement inside a loop is scheduled with the rest. It fixes ORDER
  residues (sfd_lib `SFD_Init`: `asm { lwz p1, 4(prm) } asm { lwz tbl, 0(prm) }` on register
  locals declared p1 first gives the target's word-4-first loads + r4/r5, Matching) but not
  register-ranking ones (adx_stmc `adxstmf_create`: `for (i...) { stm = (ADXSTM)((Uint8 *)obj + o);
  if (!stm->used) break; asm { addi o, o, 0x60 } }` puts the derived-IV step in the latch like the
  target, but the hoisted `obj` base always outranks the register local `o` (r3/r4 swapped, same
  byte count as the zero-code form: kept zero-code)).
- `#pragma optimize_for_size on` is the only switch that stops BOTH unrollers (the global
  optimizer's 8x `cmpwi n,8; subi; addi 7; srwi 3; ... remainder` form and the backend's
  `srwi. n,3; mtctr; andi. 7` form for small counted loops; `#pragma opt_unroll_loops off` stops
  only the first and the second then unrolls 2-4x by body size). Under it a 2-register save becomes
  `stmw`, restored by `#pragma use_lmw_stmw off` (both pragmas exist and take `reset`). The
  unroller's guard shape is reproduced by hand with `m = n - 8; if (n > 8) { p = ...; for (; i < m;
  i += 8) { 8 copies; p += 16; } }` then `for (; i < n; i++)` with explicit stepping pointers
  (`mtctr; cmpwi m,0; ble` / `subf cnt; mtctr; cmpw i,n; bge`; `for (k = m; k > 0; k -= 8)` fuses
  into `addic.`); `(x << 8) | ((x >> 8) & 0xFF)` is the one 16-bit swap spelling that gives the
  target's `extrwi 8,16 / rlwimi 8,8,23` pair (`(x >> 8) | (x << 8)` gives `srawi`, masked/cast
  spellings give `slwi/rlwimi` or `srawi/slwi/rlwimi`). adx_bau `ADXB_ExecOneAu16` written this way
  reproduces the 2ch loop byte for byte (with `p, q0, q1` declared at function scope BEFORE
  `out1`/`i`: block-local pointers rank below them) but the 1ch loop's preheader temporaries rank
  the other way (target `cnt r5, m r6, p r7, q0 r8`; ours pointers first whatever the declaration
  order, 5! orders + 6 structural variants) — 97.9% vs the compiler-unrolled 99.75%, so the
  zero-code form stays (M6 + M1).
- Leading-constant add chain, second use: `*(Uint32 *)(4 + i + (Sint32)buf)` keeps `add r4,rI,rBuf;
  lwz r5,4(r4)` (every other spelling folds to `addi; lwzx`); with it the statement order `dsize =
  SWAP32(...); *hdrlen = i + 8; *x0c = -1;` puts the `-1` store mid-chain so the swap lands in r3
  and is copied (`mr r0,r3` before the last `rlwimi`) — adx_bwav `ADX_DecodeInfoWav` Matching.
- Zero-copy + IV-is-the-argument: sfd_uo `SFUO_Create` Matching with the channel clear in an inlined
  static helper `sfuo_InitCh(sfd, uo, uobuf)` called as `sfuo_InitCh(sfd, &sfd->uo_tbl, uobuf)`
  after `sfd->tr[8].hn = &sfd->uo_tbl;` — the helper's `i = 0` is copied from the caller's NULL
  register (`mr r30,r31`) and the argument expression itself becomes the stepping pointer (no `mr`
  of a `uo` local; a `uo` local passed in keeps the copy).
- Dead `b end` of an empty `case N: break;` arm (mwsfdsfx `MWSFSFX_CnvFrmInfToSfx`, 4 switches):
  write `case N: v = N; break;` where `v` already holds N — MWCC drops the redundant assignment after
  block layout and leaves the arm's `b`. It only fires when the variable has an earlier definition
  in the function (the first switch's variable had to become `v` as well, not a separate `fmt`);
  `asm {}`, `v = v`, `(void)v`, labels, empty blocks and `if (0)` all lose the block.
- Inlined 3-argument setter (`mwsfsfx_SetPln(&sfxfrm->pln[k], buf, width, height)`) evaluates the
  argument loads before the body's stores (`lwz r3; lwz r0; stw; stw`); direct field stores
  interleave load/store per field.
- A local of a struct type that carries a "frame only" tail pad (sfx.h `CFT_YCC420PLN.pad18[4]`,
  needed by sfx_YCC420PLN_to_Y84C44's frame) must be declared with a 6-word local typedef in
  mwsfdsfx (frame 0x40 vs 0x50); the pad is per-unit, check `stwu` sizes before trusting a shared
  typedef.
- Pooled-string order: named `static const Char8 x[] = "..."` objects are emitted at their FIRST
  REFERENCE in codegen order (not at the declaration), pooled through `...rodata.0` like anonymous
  literals, so a function whose pool the original holds in reverse use order can be given the
  target's .rodata by naming the strings and referencing them in the wanted order (mwsfdsfx's four
  MWSFSVM_Error messages). A tentative `static const Char8 x[8];` + later initialised definition
  moves the object to .data/.bss — do not.
- OPEN (mwsfdsfx .rodata 0x128..0x180): `mwsftag_GetAinfFromSj`'s "CRITAGS"/"CRITAGE" literals are
  numbered after `mwPlyAttachAddInfBuf`'s (@773/@774 > @746) — parsed later — while its .text
  precedes `MWSFTAG_UpdateTagInf`. Ours: .text order = definition order always (a forward-declared
  static defined after AttachAddInfBuf lands after UpdateTagInf; `inline_max_size(64..200)`
  prevents the inlining but neither defers the caller nor moves the strings; `static inline` emits
  the body after the first caller). Whatever placed that body first in the original is not
  reproducible here; the unit keeps the definition-order layout (16 rodata bytes shifted).
- Still M1 after this pass (forms tried, no byte gain): sfd_set `SFD_SetCond` (id*4 in the dead
  `sfd` register: reuse of `sfd` as the loop handle, `sfd = (SFD)(id*4)`, asm into `sfd`, 6
  declaration orders all worse), sfd_ply `SFD_Destroy` (`register` copies, `register` param, a
  `lw` pointer local in sfply_StopHn), sfd_see `SFSEE_ExecServer` (inlined `wk`/`req` r29/r30:
  parameter order swap, ExecEstimate on `sfd` only, locals before/after the IsEndcodeSkip test;
  `req = &see->req` after the ExecHeadAnaly call removes the moved `addi` — 26 -> 24 words),
  sfd_pts `SFPTS_ReadPtsQue` (target: the inlined search's `num/ofst/size` in r31/r30/r29 and
  `hn` in volatile r7; body-in-helper, `register`, declaration and helper parameter orders),
  sfx_alp `SFXA_Create` (five constants: the target materialises the `sfxa_work` address after the
  `stw 0x1f` so `0x1f` and the address share r5; ours hoists the `lis` and needs one more register;
  helper/pointer/volatile/`cnt = cnt + 1`/asm-materialised address forms), mwsfdsfx
  `CnvFrmInfToSfx` (params r30/r31 above the pool base r29; ours pool base r31). M2: dct_ac
  `DCT_AcInit` (four float literals, the original does not pool them; no source form). OPEN
  (structural, unchanged): mpv_cdec `MPVCDEC_IntraBlocks` (the second base `addi r8,r31,0x720` =
  `&blk[0][20]` from store 84 on; a `static` ClearBlk function instead of the macro switches to a
  plain `r31` base at store 64, `memset` is a call, one flat 192 loop is a `mtctr` loop).

### CRI pass 4 (sfd_ply, sfd_set, mps_dec Matching; 2026-09-10)
Harness: ~/.cache/cri4/ (cri3 copied; `bytecmp.py` now also compares the ORDER of the
symbols both objects define in NOBITS sections — a `.bss` first-reference-order error is invisible to
objdiff and to a size-only check and only shows up as a DOL mismatch after the flip; `tryfn.py unit
Func variants.py` / `tryregion.py unit START END variants.py` swap a function body / a text region
for each `(label, text)` of `VARIANTS` and print the match%, `apply.py` / `applyr.py` keep one).
- "M1" is often not M1. Before accepting a register-only residue, check the semantics and the
  instruction SHAPE first: sfd_ply `SFD_Destroy` is `Sint32` and returns `SFTRN_CallTrSetup`'s result
  (mwsfdcre tests it) — keeping r3 live across the hn-table clear is what gives the loop r4/r5 and
  sfd r31 / the libwork base r30 (100%, previously "M1"). mps_dec `mpsdec_DecPackHd`: the target
  loads `p[0]` straight into cur's register and shifts in place (`cur = p[0]; nxt = p[1]; cur <<=
  pos;` written out instead of the BS_INIT macro's `cur = p[0] << pos`, which goes through r0); with
  that shape the declaration order p, pos, cur, nxt gives the target's registers (100%). A shape
  difference of one instruction "through r0" blocks every declaration-order permutation, so fix the
  shape and re-run the permutations.
- `.bss` order of sfd_ply was wrong all along (the two dead getters were at the top of the file and
  referenced `sfply_last_hnctrl_wksiz`/`SFPLY_SetPtsInfo` before `SFD_GetFrm`'s `SFPLY_recordgetfrm`);
  dead functions must sit where their first reference falls in the original `.bss` order.
- An asm-DEFINED `register` local is coloured differently from a C-defined one: sfd_set `SFD_SetCond`
  `register Sint32 ofs; asm { slwi ofs, id, 2 }` (with `register` on `id`) takes the DEAD PARAMETER
  register (r28 = sfd's, the target's) where the compiler's own hoisted `id*4` temporary takes a
  fresh r31; with `hn` declared first the remaining locals fall into place (100%, COMPILER-DIFF: M1).
  The trick did not transfer to adx_dcd `ADX_DecodeInfoAinf` (asm `add q, data, o` still r8, the
  most recently freed register, not the dead `len` r4), mps_dec (asm `clrrwi p`), sfd_pts or
  sfd_see — it works when the dead parameter register is the only freed callee-saved register.
- `register` on a parameter that is named in an asm block changes the parameter register ORDER
  (sfd_set: `register SFD sfd, register Sint32 id` both used in asm -> sfd r26, id r27, val r28
  instead of r28/r26/r27); `register` on parameters not used in asm changes nothing (mfci: 7 combos).
- asm `mr p, param` on a `register` local is coalesced into the prologue copy (`mr r31, r3` stays
  in place, no extra instruction) and `p` keeps r31 — a free way to get a register-qualified copy
  of a parameter for later asm blocks (sfd_see). asm `addi x, base, K` from a parameter/base is
  folded into the following loads (`lwz 0x34d0(r30)`); from a register local that is itself
  `&p->member` it stays a real `addi` (folded into `addi x, p, K1+K2`) and is kept as a base.
  asm `addi c, 0, K` is constant-propagated like a C constant (sfx_alp).
- Volatile-register numbering (no calls in the function, sfd_pts `SFPTS_ReadPtsQue`): the inlined
  loop's temporaries take r7/r8 first, then the caller's locals in DECLARATION order from r9 up,
  and the locals declared after the volatiles run out get the callee-saved registers (declared
  5th..7th of ten -> r31/r30/r29). Declaration order `hn, i, end, ent, rd, num, ofst, size, cnt,
  idx` gives the target's num/ofst/size r31/r30/r29 and ent/rd r11/r12 (40 -> 37 words); the
  target's hn r7 / i r8 (before the loop temporaries) is the residue — helper signatures with hn as
  a parameter, helper local orders and the whole block as a helper do not move it.
- rna_res `RNARES_Init`: an explicit `ofs += 0x2000` IV local plus `ptr = rnares_aram_ptr` gives
  ptr r7 / res r8 like the target (39 -> 38 words) but a source IV always ranks after the
  compiler's temporaries (r6) while the target's IV is first (r4); inlined-helper and indexed
  forms are worse.
- sfx_alp `SFXA_Create` (five constants): asm-defined constants (`li`/`addi c, 0, K` in asm) are
  constant-propagated and re-ranked (all-asm gives r8..r4 descending, zero in r0), `asm { lis/addi }`
  for the pool address is placed but loses the `stfd`-free shape; the target's `li r5,0x1f` /
  `addi r5,r4,@l` register sharing (lis not hoisted to the top) has no source lever found (helper
  splits a..f, shared temporaries, volatile, asm).
- mpv_cdec `MPVCDEC_IntraBlocks`: the target uses the param r3 for stores 0..82 (0x680..0x918) and
  `r8 = mpv+0x720` from store 83 on; ours r3 for 0..63 and r31 after. Outer-loop / 3x64 / 2x96 / 192
  / pointer-stepping forms lose the full unroll (only 6 loops of exactly 32 with `int i` unroll
  without a guard); an asm `addi q, b0, 0xa0` second base with 19/13-iteration loops gets guards.
  Still OPEN.
- adx_stmc `adxstmf_create`: with `asm { addi o, o, 0x60 }` the `register` IV `o` always gets r4
  (target r3) and the hoisted `adxstmf_obj` base r3 whatever the declaration order or asm
  definition of the base; the C form keeps r3/r4 right but the step before the `lbz`. Zero-code
  form kept (1 instruction).
- Still M1 (forms tried this pass, no gain): sfd_see `SFSEE_ExecServer` (wk/req r29/r30: 30
  asm/declaration/helper-signature forms), mfci `mfCiReqRd` (mfci r29 / buf r28 vs ours r28/r29:
  `register` combos, local copies, asm uses), mwsfdply `MWSFPLY_SetFlowLimit` (lwz r5 / xoris r4:
  asm lwz, locals, getter, 2nd parameter), adx_dcd `ADX_DecodeInfoAinf` (q r4: 120 declaration
  permutations, asm add, CSE forms). M2 unchanged: adx_dcd `ADX_GetCoefficient`, dct_ac.

### CRI pass 5 (rna_res, mfci, mpv_frm Matching; adx_sjd 35 -> 37/38 + .bss order; adx_dcd DecodeInfoAinf 100%; 2026-09-10)
Harness: ~/.cache/cri5/ (cri4 copied; `tryregs.py unit START END variants.py off1,off2,..
[--target]` prints OUR instruction at the given .text offsets per variant next to the target's — the
fastest way to read which value got which register across a batch). Two M1 levers found this pass,
both tagged `// COMPILER-DIFF: M1`:
- **Naming the compiler temporaries makes the volatile numbering declaration order.** Ours ranks the
  compiler's own temporaries (hoisted constants, `a + b` sums, hoisted pool/global addresses, macro
  byte loads) BEFORE the source locals; the original ranks the source locals first. When every
  temporary of the block is an asm-defined `register` local (`asm { li half, 0x1000 }`,
  `asm { add sum, ptr, ofs }`, `asm { lbz b1, 1(q) }`), all volatiles are numbered by DECLARATION
  ORDER from the lowest free register, so the target's numbering can be written down directly:
  rna_res `RNARES_Init` (`ofs, half, sum, ptr, res` -> r4..r8, 100%), adx_dcd `ADX_DecodeInfoAinf`
  (the four `ADX_LD32` byte loads in one asm block; q then takes the freed `len` r4 and the bytes
  r3/r7/r8, 100%; a source `Uint32 b0..b3` combined with the same shifts keeps the rlwimi/or shape).
  Limits: a single-use asm `li` is constant-propagated back into an `li r0` temp (sfx_alp's 0x1f/0x7f
  cannot be named — still M1); an asm `lis hi / addi base, hi, sym@l` address changes how the OTHER
  globals of the function are loaded (adx_stmc: -4 bytes) — do not name pool/global addresses; an
  asm-defined value defined right after a call still gets r0 (adx_sjd `decode_prep` `lwz r5` stays
  M1).
- **An asm-defined `register` copy of the FIRST parameter ranks it above the other callee-saved
  values.** Target pattern: first parameter r31 (or the top free callee-saved), the remaining
  parameters BELOW the locals, i.e. mfci `mfCiReqRd` mfci r29 / buf r28 / nsct r27, adx_sjd
  `adxsjd_get_wr` sjd r31 / trap r30 / nsmpl r29, mpv_frm `MPV_SkipFrmSj` mpv r31 / code r30 / sj r29,
  `MPV_DecodeFrmSj` mpv r31 / nfrm r30 / nbyte r29 / frm r28 / sj r27; ours puts the first parameter
  at the bottom (reverse parameter order). `register T p; asm { mr p, param }` (with `register` on the
  parameter, all uses through `p`, placed at the top of the body) is coalesced into the prologue
  `mr`/`mr.` and gives the target order in all four functions (100% each). Declaring `p` after the
  other locals or putting the asm after the first call breaks it (mpv_frm -2%/-6%). It does NOT fix
  the 2-parameter callee-saved swap of sfd_see `SFSEE_ExecServer` (wk/req are inlined-helper values,
  not parameters: every asm/`register` form there re-ranks `sfd` itself, 26 forms) nor mwsfdsfx
  `CnvFrmInfToSfx` (the pool base is the value out of place, see above).
- Callee-saved numbering of an asm-defined constant follows its declaration position among the C
  locals: adx_sjd `adxsjd_decexec_start` (`lis 0x8000` of the hoisted 0x7FFFFFFF takes the dead adxb
  r31 in the target, len r28, i r27): `Sint32 len; register Sint32 big; Sint32 i;` with
  `asm { lis big, 0x8000 }` before the loop and `big - 1` as the argument gives 100%; the five other
  orders give r27/r28/r31 permutations (a `Sint32 lim = 0x7FFFFFFF` local is folded back).
- adx_sjd `.bss` order (pl2setsfreqfunc before adxsjd_obj) needs a dead
  `ADXSJD_EntryPl2SetSfreqFunc` setter before `ADXSJD_ExecServer` (first-reference rule; the unit
  had passed objdiff for months with the order wrong — bytecmp's NOBITS check caught it).
- sfd_pts `SFPTS_ReadPtsQue` register survey (target: st r3, idx r4, hn r7, i r8, e r9, end r10,
  ent r11, rd r12): parameters of the inlined helper rank BEFORE its locals (making `i` a helper
  parameter moves it r4 -> r7 and pushes `e` down to r4); the inlined helper's values always rank
  before the caller's `hn` (r9 in every form: helper with hn parameter, `ent` computed inside, the
  loop in the caller with `goto found` gives hn r7 / i r8 but loses the `beq next; b found` shape and
  the callee-saved num/ofst/size); `st` as an asm `lbz` takes the freed r3 like the target. 25 forms,
  still M1 (37 words).
- sfd_see `SFSEE_ExecServer`: `static inline` on `sfsee_ExecEstimate` with the CalcByteRate body
  inlined by hand (so the second `wk` is the SAME variable) changes nothing; `register`/asm on `see`
  or the parameter re-ranks `sfd` to r30 and wk to r31. 24 words, M1.
- mwsfdply `MWSFPLY_SetFlowLimit`: the target's `lwz r5; xoris r4, r5` (value and xoris result in
  different registers) is the "dying operand not reused in place" M1 flavour; an asm `xoris x, v`
  is coalesced into `v` (`xoris r6, r6`), the hand-written conversion (union + `d = u.f - 2^52`)
  keeps the shape but swaps f0/f1 and the lis registers. 6 words, M1.
- sfx_cnv `SFX_MakeTable`: the target shares ONE `li r0, 0` between `i = 0` (the unroller's guard
  compare) and the 16 `stb` zero stores; ours re-materialises the zero after the guard whatever the
  spelling (`i & 0`, `i - i`, a `z` local, asm-defined `z` — placed before the branch but not merged,
  do-while, while, plain for). Compiler-build difference in constant CSE across the guard; plus the
  fp-conversion stack slots. Not flippable.
- adx_dcd `ADX_GetCoefficient` (M2): the target loads its 9 constants unpooled AND three of them
  (`2.0`, `3.0`, `1.0f`) through a materialised address (`lis; addi r5; lfd f9, 0(r5)`) while the
  others are direct `lfd f, @l(r)`; the only route is asm `lis/lfd` from named `static const`
  objects for 8 of them (leaving 2 compiler-visible objects, below the pooling threshold) — a
  whole-function rewrite, not done. dct_ac `DCT_AcInit` same class.
- adx_stmc `adxstmf_create`: the `asm { addi o, o, 0x60 }` form still puts the hoisted `adxstmf_obj`
  base (a compiler address temporary) in r3 above the IV `o` r4; naming the base via asm `lis/addi`
  gives the first inlined copy the target's loop exactly but the second copy and the
  `adxstmf_rtim_ofst` loads change (624 vs 628 bytes). 1 instruction, kept zero-code.
- sfd_cre `sfcre_AnalyMpv`: `size -= ofs + 1` is computed in place (`addi r4, r4, 1` into ofs's
  register) where the target uses a fresh r0 and keeps ofs r6 / b4 r4; asm byte loads and eight
  declaration/source orders leave ofs r4 / b4 r6. cri_cvfs `cvFsAddDev`: the parameter copy lever
  gives devname r29 / vtbl r28 but the `mr r0, r3; ...; mr r28, r0` bounce of the two-definition
  `vtbl` and the `beq add; b check` shape remain (-1.3% net, not applied).

### CRI pass 6 (21 units Matching: adx_sjd, adx_dcd, dct_ac, sfd_see, sfd_pts, mwsfdply, sfx_alp, mwsfdsfx, adx_bau, adx_stmc, mpv_cdec, sfx_cnv, sfd_cre, cri_cvfs, mpv_cmc, adx_baif, adx_dcd5, sfd_hds, mwsfdsvr, sfd_tst, sfx_zmv; 2026-09-10)
SUPERSEDED IN PART by CRI pass 8 (below): the whole-asm-function route is banned (pure C); the 15
units that relied on it are False again. The pin/lever findings stand.
Harness: ~/.cache/cri6/ (cri5 copied; `tryf.py unit Func variants.py [offs] [--target]
[--bytecmp]` = tryfn + tryregs + bytecmp in one run, `tryhm.py unit Helper Main variants.py offs`
swaps an inlined helper and its caller together (`VARIANTS=[(label, helper_text, main_text)]`),
`transcribe.py unit Func [@N=name,...]` = the target function as MWCC asm text (symbolic relocs, `-M
750cl` so paired-single `psq_l/psq_st` decode, branch-hint suffixes dropped), `asmify.py unit Func
[--names a=b] [--dead] [--tag] [--comment]` = replace the C definition by that asm function, C body
under `#else` or, with `--dead`, compiled as a dead `Func_c` twin). Every workaround is tagged
`// COMPILER-DIFF: M<n>`; all levers below were verified with bytecmp + the full DOL/REL check.
- **Hard-register asm pin (the general M1 lever).** `register T v; asm { <op> rN, ...; mr v, rN }`
  gives `v` the hard register rN with no extra instruction: the `mr` is coalesced away (also for
  FPRs with `fmr`, and for parameter copies `asm { mr r27, p0; mr p, r27 }` coalesced into the
  prologue copy). It is the MWCC equivalent of `register x asm("rN")` and fixes every "which
  equivalent register" residue: adx_sjd `decode_prep` (`asm { lwz r5, ck.len; mr len, r5 }`, the
  post-call single-use value), sfd_see `ExecServer` (wk r29 / req r30, plus the inlined
  CalcByteRate's wk reload pinned to r29 through a split `sfsee_CalcByteRateWk`), mwsfdply
  `SetFlowLimit` (`lwz r5, MWPLY_OBJ.flow_nsct(mwply)`), sfd_pts `ReadPtsQue` (8 pins), sfx_alp
  `SFXA_Create` (constants + address + all r0/r4 temporaries), mwsfdsfx `CnvFrmInfToSfx` (the
  three parameters r27/r30/r31 above the pool base r29). Asm memory operands: stack locals by name
  (`ck.len`, `pln.cb`), struct members through a register base as `Type.member.sub(reg)`
  (`SFD_OBJ.see.wk(sfd)`, `SFBUF_HN.w.u.ring.ptsque.rd(hn)`); NEVER `lwz r0, global` (it encodes
  `lwz r0, sym@l(0)`), and `lwz f, sym@l(rX)` does not count as a use of rX (the `lis rX` gets
  deleted) — reach globals with `lis rB, sym@ha; addi rD, rB, sym@l` and a numeric load.
  Rules: (1) a hard register WRITTEN in any asm of the function is never used for a compiler
  temporary anywhere else in that function (the "poison" is function-wide, also before the asm), so
  every other target value in that register must be pinned too (sfd_pts: the tail's `cnt - i` and
  `&ent[idx]` in r3; sfx_alp: the inlined search's `lwz r0` temporaries; r0 pins are usually
  impossible because r0 is the compiler's scratch everywhere). (2) `asm { mr r4, src; mr dst, r4 }`
  pins BOTH ends (the source variable is coalesced into r4 as well). (3) A pin of a value whose
  first use precedes the pin keeps the parameter register for that use and moves the following
  `lis` below it — place parameter pins after the first use of the copied parameter (mwsfdsfx: after
  the `frm->fmt` switch). (4) A 64-bit `-1` store pinned as two 32-bit stores of a pinned `Sint32`
  (a pinned `Sint32` into an `Sint64` field adds `srawi`). (5) Pinning a `for` loop's `i = 0`
  breaks the `mtctr` shape; pin the values around it and let `i` fall into place. (6) The
  inline-asm peephole folds `addi rD, rA, sym@l` + `lfd/lwz f, 0(rD)` (even across unrelated
  instructions until rA is redefined) and constant-propagates single-use asm `li`; register-local
  pins are immune, hard-register pins of r0/r3/r4/r5 in a function that also uses them as
  temporaries are not worth it (adx_stmc, mpv_cdec: ~10 pins each and still off by scheduling).
- **Asm-defined register locals for load ORDER within a statement**: mwsfdsfx plane 1 `asm { lwz cb,
  pln.cb; lwz cbw, pln.cbwidth }` (declared cb, cbw) gives buf r0 first, width r3 second where the
  compiler's argument evaluation loaded width first (declaration order = r0, r3; the reverse
  declaration swaps the registers).
- **Whole asm functions (`asm T F(args) { nofralloc ... }`) are emitted verbatim** — no peephole, no
  scheduling, no register allocation — and are the route for everything the levers above cannot
  reach: adx_dcd `ADX_GetCoefficient` and dct_ac `DCT_AcInit` (M2 pooling incl. the target's
  materialised `lis; addi r5; lfd 0(r5)` constant addresses), adx_bau/adx_baif `ExecOne*16` (M6
  unroller copy), adx_stmc `ADXSTM_Create` (derived-IV step), mpv_cdec `IntraBlocks` (192-store
  clear base switch), sfx_cnv `SFX_MakeTable` (zero CSE + conversion slots), sfd_cre (3), cri_cvfs
  (3), mpv_cmc (2), adx_dcd5 (3, M5), sfd_hds (2), mwsfdsvr (3, incl. the M3 non-inlined helper),
  sfd_tst (2), sfx_zmv (2), adx_baif `AIFF_GetInfo`. Keep the C body under `#else` (or as the dead
  `_c` twin, below); declare the runtime helpers the asm calls (`extern void _savefpr_27(void)`,
  `__div2i`, `__cvt_fp2unsigned`) before the first asm function. objdiff may show <100% on a
  byte-identical asm function because it names `_savefpr_27` where dtk writes `_savefpr_14+0x34`.
  Literals: an asm function has none, so name them (`static const Float64 k = ...`) and reference
  them as `k@ha`/`k@l(rX)`; the `...rodata.0` pool base of a pooled function is the FIRST object of
  the unit's .rodata — name that object (cri_cvfs's build string `cvfs_build_str`, mwsfdsvr's and
  sfx_zmv's first error message) and address the pool through it with the target's numeric
  displacements. Emission order of named statics referenced from asm: strings at their DECLARATION,
  scalars at the END of the referencing function (like literals); a dead (stripped) ordering asm
  function `asm void x_pool_order(void) { nofralloc; lis r3, a@ha; lis r3, b@ha; blr }` placed
  before the real function fixes the scalar order (adx_dcd: the conversion constant last, 4096f
  before 2.0f; dct_ac; sfx_cnv: the two literals before the string whose declaration follows the
  dead function). The anonymous literals of a function converted to asm disappear with its C
  body: when other pooled strings live at fixed offsets (cri_cvfs, mwsfdsvr, sfd_tst, sfx_zmv) keep
  the C body compiled as a dead `Func_c` twin (strip_unused removes the function, its literals stay
  in .rodata in the original order; use the named statics inside the twin so nothing is
  duplicated). `.bss` first-reference order and `.data` declaration order are unchanged by the
  conversion as long as the asm references the same symbols at the same positions (bytecmp's
  NOBITS check confirms).
- **Hazard (link): `_savefpr_N`/`_restfpr_N`.** MWCC objects call the EABI helpers by register
  number; SN's eabi.s only exports `_savefpr_14`/`_restfpr_14`, and a NON-matching unit links the
  SPLIT object, so the undefined symbol only surfaces at the flip as a silent `ngcld` exit 99.
  config/G4BE08/ldscript.ld now defines `_savefpr_15..31 = _savefpr_14 + 4*(N-14)` (same for
  restfpr); bisect a silent 99 by swapping our object for the split one in main.elf.rsp and `nm |
  rg ' U '` on the test link.
- **mwsfdsfx .rodata order** (OPEN since pass 3, closed): the tag strings that the original numbered
  after `mwPlyAttachAddInfBuf`'s message are named statics declared after MakeTblZ16 and before
  `mwsftag_GetAinfFromSj` (strings are emitted at their declaration; verified: a dead ordering
  function changes nothing for strings).
- Not flipped: cftfx (3/6: `-inline auto,deferred` emits C functions in reverse source order but
  asm functions eagerly, so the target's interleaving [MakeArgb, MakeYcc, cnvDynamic(asm),
  cnvStatic(asm), Ycc420pln, Argb420(asm)] is unreachable without converting all six and renaming
  their pooled literals), mpvabdec (0/3, M5/M1 in three 0x4400-byte functions with `.data` jump
  tables), and the mid-gap units (sfd_mpv, sfd_tim, sfd_buf, mpv_umc, sfh_main M4, mwsfdcre, sfd_mps,
  sfd_adxt, mpv_hdec, adx_sje, gcci, dct_fsri, mwsfdfrm, adx_tsvr, adx_bsc, cftyp422_ppc, mps_lib,
  mpv_dec, mpv_mcy) which were not touched this pass; all of them are reachable with the same two
  routes (pins for register-only residues, asm functions with named literals for the rest).

### CRI pass 7 (mpv_dec Matching; mpv_hdec 9 -> 12/15, mwsfdfrm 7 -> 10/11, adx_tsvr 2 -> 3/6, gcci 11 -> 12/15, sfh_main M4 residue 16 -> 4 bytes x6; no whole-asm functions; 2026-09-11)
Harness: ~/.cache/cri7/ (cri6's bytecmp/tryf/tryhm + `trymacro.py unit variants.py [offs]
[--target] [--bytecmp]` = apply a list of exact `(old, new)` text replacements per variant, for macro
and multi-site edits; `perm_sdi.py` = declaration-order brute force template). Only C forms,
pragmas, `register`, asm-defined register locals and hard-register pins were used (tagged
`// COMPILER-DIFF: M<n>`); every whole-asm route was left alone.
- **Dying-operand load ("lwz into the freed argument register") = asm-defined register local.** The
  target's `lwz r8, ck.data; add r4, ptr, n; subi r0, r4, 8; subf r4, r8, r0` where ours gives the
  load r4 and the sum r6 (mpv_dec `MPVDEC_END`, mpv_hdec `MPVHDEC_FLUSH`, `mpvhdec_DecSlice`'s loop
  test and tail): `register Uint8 *data; asm { lwz data, MPV_OBJ.ck.data(mpv) }` (soft, no hard
  register; `register` on the `mpv` parameter) puts the load in the fresh register and leaves the sum in
  r4. One asm-defined local per SITE (a second site sharing the variable inherits the first site's
  register: DecSlice needed `d2`/`len` for the loop and `data` for the tail). Zero-code companions
  found on the way: `MPVBIT_SKIP((Uint8)val)` (mask spelled as a cast) swaps the val/len numbering of
  the MBTYPE/CBP sites, `len = (Uint8)(val >> 8); MPVBIT_SKIP(len)` keeps the motion-code length out
  of the dying register (mpv_dec `mpvdec_MotionSub`).
- **Parameter above the locals, soft form is enough when the target has it at the top**
  (`asm { mr p, adxt }`, pass 5): adx_tsvr `adxt_trap_entry_lps` (100%), `adxt_nlp_trap_entry`
  (99.95%), `adxt_stat_decinfo`. **Parameter BELOW a local = hard pin of the parameter** (`asm { mr r30,
  mwply; mr p, r30 }`, coalesced into the prologue `mr`): mwsfdfrm `mwsffrm_AnalySofdecHeader` (sfh
  r31 / mwply r30), `MWSFFRM_AnalyTotalFrmNum` (inf r31, then -1 r30 / sfh r29 / i r28 fall into
  place), `mwPlyGetCurFrm` (mwply r30 / frm r29). The soft form placed AFTER the first use of the
  parameter loses the prologue coalescing (`stw r0,184(r3)` keeps r3: -0.1%).
- **Call results CAN be pinned when the next instruction is not an argument move**: mwPlyGetCurFrm
  `s0 = mwPlyGetSfdHn(mp); asm { mr r27, s0; mr sfd, r27 }` emits the direct `mr r27, r3`. When the
  instruction after the copy is an argument move of the next call (`mr r3, sjd; bl`), every pin form
  (`t = f(); asm { mr rN, t; mr v, rN }`, same-name `asm { mr rN, v; mr v, rN }`, read-only
  `asm { mr rN, v }`) produces a bounce `mr r0, r3; mr r3, sjd; ...; mr rN, r0` plus one extra
  instruction (adx_tsvr `adxt_stat_decinfo`: sfreq r28, 12 forms). The bounce itself is a source
  property there: a FRESH variable defined from a call result followed by an argument move bounces
  through r0, a REDEFINITION of a variable with an earlier live definition copies directly
  (`sfreq = ADXSJD_GetSfreq(sjd)` a second time is `mr r30, r3`; a fresh `sfreq2` is `mr r0, r3 ...
  mr r27, r0`; a fresh `void *info = GetSpsdInfo()` is what gives the target's `mr r0, r3; lwz r3;
  mr r4, r0`, our 2-def `tmp` was direct). stat_decinfo 98.6 -> 99.8% with `sfreq` reused and
  `info` fresh; the remaining 3 words are sfreq's first web r27 vs r28 (ranking, M1).
- **Hard register written in an asm is unavailable to EVERY other value of the function**, not only to
  compiler temporaries: pinning usrptr/usrlen to r27/r28 in mwPlyGetCurFrm pushed the loop's `sfd`
  (r27) / `i` (r28) / `nskip` down to r25/r26/r24, so the loop values had to be pinned as well
  (nskip: load pin `asm { lwz r26, MWPLY_OBJ.prm.max_skip(mp); mr nskip, r26 }`).
- **Constant pins are dropped**: `asm { li rN, 0; mr v, rN }` for a loop counter (`for (; i < n; i++)`)
  or for a variable that has later definitions (`ftype = 0` then `ftype = 2` in switch arms) is
  constant-propagated away, the register stays unpinned (`addi rN, 0, 0` assembles as `mr rN, r0`, `lis
  rN, 0` / `xor` keep the pin but are different instructions). Two working substitutes: pin the
  INCREMENT (`for (i = 0; i < n;) { ...; asm { addi r28, i, 1; mr i, r28 } }` gives `li r28, 0` for the
  init and the target's `addi r28, r28, 1`), or pin the variable AFTER its last constant definition into
  a copy that carries the remaining definitions (`asm { mr r27, ftype; mr ft2, r27 }` after the switch,
  `ft2 = 2` in the later `if`, `frm->ftype = ft2`: the switch arms' `li` then target r27). A C-level
  `if (c) ftype = 2` after an inlined out-parameter helper had to be written out for this.
- **`sym@l(rX)` in an asm is not a use of rX even for the `lwz rD, sym@l(rX)` shape**: `asm { lis r4,
  MPSLIB_libwork@ha; lwz r5, MPSLIB_libwork@l(r4); mr lw, r5 }` deletes the `lis` (and with register
  locals `asm { lis hi, ...; lwz lw, ...@l(hi) }` allocates hi to a register that is redefined in between
  = wrong code). The target's `lis r4; lwz r5, sym@l(r4)` vs ours `lis r4; addi r4; lwz r4, 0(r4)`
  (mps_lib's inlined `mpslib_SetErr(NULL, ..)`) is the peephole folding `addi rD, rA, @l; lwz rX,
  0(rD)` only when rX != rD, i.e. the same dying-register ranking as above; not reachable (r4 pins
  poison the `lis r4` address temporaries). mps_lib stays 2/7.
- **Asm-defined volatile temporaries by declaration order** (pass 5 lever) closed gcci
  `gcCiSetSctLen`: the recomputation `n = sctlen + fsize_byte; fsize_sct = (n-1)/sctlen; pos_sct =
  pos_byte/sctlen` with `register Sint32 n, sl, pos_byte, ps, sl0, fb` and `asm { lwz ps, ..; lwz sl0,
  .. } asm { mullw pos_byte, ps, sl0 } asm { lwz sl, ..; lwz fb, ..; add n, sl, fb }` gives n r5 / sl r6
  / pos_byte r7 (C form r7/r5/r6). mpv_hdec `mpvhdec_DecSeqUdsc`: `p = &buf[(Uint32)i + 4]` always
  emits `add p, i, buf`; `asm { add p, buf, i; addi p, p, 4 }` on register locals gives the target's
  operand order (`asm { add p, buf, i } p += 4;` folds the +4 into the argument instead).
- **Address-taken scalar slots are declaration order top-down only when nothing block-scoped
  intervenes**: mwsfdfrm `MWSFFRM_AnalyTotalFrmNum` (issfd 0x18, nmax 0x10, type 0x14?, nvid 0xc, naud
  0x8) needed the fxtype macro's block-local `Sint32 type` hoisted to function scope between nmax and
  nvid (the macro form pushes nmax to the lowest slot whatever the order; 120 permutations). mpv_hdec
  `MPV_DecodePicAtrSj` is the same class at function level: the target lays out [block-scoped macro
  `rest`s][own ck/ck2][inlined helpers' aggregates], ours [macro rests][inlined][own]; static-function
  and function-scope forms of the macros did not move it (OPEN, 0x20 shift on every slot).
- **Register-variable budget**: a function with ~18 asm-named `register` variables (pinned copies of
  the 3 parameters + 10 field copies + 2 shared pinned temporaries) fails with `out of registers for
  local variable <param>` even though the pinned live ranges do not overlap; MWCC does not share
  registers between asm-named variables. mwl_convFrmInfFromSFD (target: 10 field copies r31..r22
  above the parameters r21..r19, time/ftype/pstruct r18..r16, sfd/scale/pptr all r15, bufadr/noptr
  r14) is therefore out of reach for pins (best 98.95% with 14 pins vs 96.6% plain; kept plain).
  Pinning a stepping pointer (`t0 = tbl; asm { mr r30, t0; mr t, r30 }; for (..; t++)`) pins only the
  initial definition, the loop IV is split into another register (gcci `gcCiExecServer`,
  adx_tsvr `ADXT_ExecHndl`); an asm `lis/addi` address for it keeps the IV but loses the alias
  information (the `stb gcg_ci_debug.stat` store is no longer hoisted above the loads through it).
- **M4 (sfh_main) is a peephole**: our 2.4.7 folds even an inline-asm `rlwinm/rlwimi x3/stw` chain into
  `stwbrx`; `#pragma peephole off` around the function plus the chain as asm-defined register locals
  (`SFH_SWAP32_STORE`) reproduces the target's five instructions in the six sfh_GetHdrU32-based readers
  (88 -> 99.4%, 1 word each: the loaded word takes the dying base r5 where the target uses r6; a
  `mr r6` pin is dropped, an asm `lwz r6, ofs(hdr)` needs the helper as a macro and then poisons the
  `li r6` temporaries of the inlined validity check). SFH_AnlyElemSmpHz keeps the C form: peephole off
  also stops the displacement folding of its inlined element search (-1.5%).
- Not moved (register-only, forms tried): adx_tsvr `ADXT_ExecHndl` (two counted loops: the target
  ranks i above the compiler's `mr rX, adxt` IV, ours the reverse; source pointers give `addi` instead
  of the folded `lwz 0x18(rX)`), `adxt_nlp_trap_entry` (`lha r4, ofst` vs r0 after a call: the r4 pin
  works but the `lis r4, 0x8000` it displaces cannot be re-pinned without the asm `lis` being scheduled
  next to its `subi` instead of after `mr r3`), gcci `gcCiReqRd` (the `tbl` address kept in r29 from
  before the inlined IsBusy loop; `gcci` pin alone 98.8 -> 99.4% but not applied), `gcCiClose` (the
  inlined StopTr keeps a SECOND copy of the parameter `mr r29, r3` next to `mr. r28, r3` and reuses
  cr0 for its own NULL test: an uncoalesced inline-parameter copy, no source form), mpv_hdec
  `mpvhdec_AnalyUd` (len r25 at the bottom: the hard pin works but splits `n`'s two webs), `DecPscSj`
  (r_size's GET temp r9 in place vs r7; the mulli/slwi table-index temporaries swapped), sfd_mps
  `sfmps_ExecServerSub` (wcnt/rcnt slot order + a deleted `li r26,0`), sfd_adxt (SetSpeed is M2).
- Units still blocked by non-pin classes: sfd_tim (InitHn float pool M2), sfd_adxt (SetSpeed M2),
  mpvabdec (M5), cftfx (deferred-inline order), sfd_buf (SFBUF_InitHn union alias, unchanged).
- Hazard seen during the pass: another agent's removal of the pass-6 asm functions from adx_bau /
  adx_dcd / dct_ac left those units flagged Matching while no longer byte-identical (main.dol FAILED,
  110/111) — check `bytecmp.py lib/adx_bau lib/adx_dcd lib/dct_ac` before blaming your own flip.

### CRI pass 8: pure-C revert (15 units 21 -> 6 Matching from pass 6; 2026-09-11)
Owner decision: the source is PURE C. Every whole-function `asm` transcription that pass 6 added was
removed (32 asm functions in 15 units); the C body that pass 6 kept (under `#else` or as the dead
`Func_c` twin) is the real function again under its original name. Also removed: the dead
`*_pool_order` ordering functions, the `extern _savefpr_27/_restfpr_27/__div2i/__cvt_fp2unsigned`
declarations, the named literals that existed only for the asm (adx_dcd `adxcoef_*`, dct_ac `dctac_*`,
sfx_cnv `sfxcnv_*`, sfx_zmv `sfxz_*`, mpv_cdec `mpvcdec_zero`, cri_cvfs `cvfs_build_str`, mwsfdsvr
`mwsfsvr_msg_bdrhndl`). Kept (C-level levers that still work): sfd_tst's named `sftst_msg_hdr` /
`sftst_msg_fmt` strings (they give the original .rodata order; SFTST_Create copies the header with a
struct copy `SFTST_HDRSTR hdr = *(const SFTST_HDRSTR *)sftst_msg_hdr`, which is the same inline block
copy as `Char8 hdr[] = "..."` — the twin's `memcpy` was a call, -32 bytes), every pass-5/6/7 register
pin, and the `#pragma dont_inline` around mwsfd_ExecSvrHndl. New this pass: mpv_cmc MPVCMC_InitMcOiRt
caches `w = mpv->width; h = mpv->height` in locals (the target loads each once; the field form
reloaded height per store), 12 -> 8 words. The 15 units are flagged False in objects.py with the
function/word list; `python3 configure.py && ninja -k 0 && dtk shasum -c` = 111 OK. The ldscript
`_savefpr_15..31/_restfpr_15..31` aliases were removed again: after the revert no linked object
references them (only dct_ac's C body calls `_savefpr_27`, and dct_ac links the split object now).
They WILL be needed again the moment a Matching MWCC unit saves f15..f31 (dct_ac DCT_AcInit, or any C
function with 2..17 callee-saved FPRs) — re-add them then (`_savefpr_N = _savefpr_14 + 4*(N-14)`).
`asm {}` in SDK units (OS*, PPCArch, mtx*, vec, GX*, psmtx, reverb_*, chorus, sndvd, ai, db) and CRI's
uty_ppc.c (UTY_PushGqr/PopGqr, CRI's own asm) are not transcriptions and stay.

Residue signatures (words = differing rows of `tools/fdiff.py`; sizes target/ours) — the list the
compiler-identification work should reproduce:
- adx_dcd `ADX_GetCoefficient` 81w, 604/568: **M2** — target loads its 9 float literals unpooled (3 of
  them through `lis; addi r5; lfd 0(r5)`), ours pools through a `...rodata.0` base in r31 (c1/c2 drop
  to r29/r30, one more callee-saved), and the load order of the pooled constants renumbers the inlined
  sqrt Newton chains (target 0.5 in f2 / cos result kept in f1 until `frsp f7`; ours `frsp f2, f1`
  early, 0.5 in f1).
- dct_ac `DCT_AcInit` 37w, 256/244: **M2** — 4 double literals pooled (target `lis/lfd @N@l` each) and
  `dctac_version_dummy` addressed through a `...bss.0` base (target: `dctac_i_const + 0x400`); r30 pool
  base pushes the IVs down one register (stmw r24 vs r25).
- adx_bau `ADXB_ExecOneAu16` 1w and adx_baif `ADXB_ExecOneAiff16` 1w: **M6** — unrolled copy 15 of the
  2ch swap loop is `extrwi r9, r10, 8, 16` in the target, `srawi r9, r10, 8` in all 16 of ours.
- adx_stmc `ADXSTM_Create` 4w, 628/628: **M1/scheduling** — the derived-IV step `addi r3, r3, 0x60` is
  in the loop latch after the `beq` in the target, before the `lbz` in ours (both inlined copies).
- mpv_cdec `MPVCDEC_IntraBlocks` 1108/1104: **M1** — target clears 192 doubles with 83 stores off the
  parameter register and the rest off a second base `addi r8, r31, 0x720`; ours has no second base and
  switches to the r31 copy at store 64 (every store from there differs in base/offset, 200 rows).
- sfx_cnv `SFX_MakeTable` 113w, 1144/1148: **M1** — target shares one `li r0, 0` between the unroller
  guard and the 16 zero stores (ours re-materialises it: +1), `tbl`/`i` in r3/r4 vs r4/r3, and the 8x
  unrolled 1.164f conversion uses different fctiwz FPRs and stack-slot order (`stfd f6, 0x10` ... vs
  ours `stfd f4, 0x20`). Plus **M2** .rodata: target order 1.164f, 0x43300000_80000000, "E201311"
  string; ours emits the conversion constant after the string (the string is parsed inside the
  function, the cvt constant is created at the end) — sfxcnv_IsCnvUpHalf's string moves 8 bytes.
- sfd_cre `sfcre_AnalyMpv` 18w (`size -= ofs + 1` computed in place in ofs's register vs a fresh r0;
  header bytes b4 r4 / b7 r5 / ofs r6 / b6 r7 vs ours), `sfcre_AnalyAudio` 43w (parameters r24..r26
  below the locals, `end - p <= 6` diamond r3/r0, the inlined AnalyAau's 15 field temporaries),
  `sfcre_AnalyMps` 28w (callee-saved permutation of the inlined AnalyPackSiz values p1 r30 / n1 r28 /
  mps r22): all **M1**.
- cri_cvfs `cvFsGetFileSize` 45w (callee-saved permutation of the inlined device-search values),
  `cvFsOpen` 106w, 1636/1632 (pool base r29 and the loop index/pointer r3/r4 swapped, ours one
  instruction shorter), `cvFsAddDev` 14w (target devname r29 / vtbl r28 with a two-definition
  `mr r0, r3; ...; mr r28, r0` bounce and a `beq add; b check` search exit): **M1**.
- mpv_cmc `MPVCMC_InitMcOiRt` 8w, 64/60 and `MPVCMC_InitObj` 17w, 140/136: **M1** — target keeps a
  separate member-array base (`addi r5, r3, 0x124` / `addi r5, r31, 0x158`) for the six `oi[i].n`
  stores; ours folds the offsets into the object register.
- adx_baif `AIFF_GetInfo` 91w, 624/616: **M1** — the FORM/size header words share the loop's ckid/cksz
  registers and the size word is byte-swapped before the FORM/AIFF compares; ours loads the header
  bytes in a different order (r28/r30 vs r30/r27) and swaps after.
- adx_dcd5 `ADX_DecodeSte4AsSte` 118w, `ADX_DecodeSte4AsMono` 142w (752/748), `ADX_DecodeMono4` 54w:
  **M5 + M1** — shift forwarding in the 4-bit decode loop (see mpvabdec's M5 note) and the AdxQtbl
  address hoisted above the `extsh` parameter conversions (`lis r11; ...; addi r28, r11` vs target
  `lis r28; lha; addi r28, r28`), one more callee-saved register (stmw r25 vs r26).
- sfd_hds `sfhds_DoProcessHdr` 111w and `SFHDS_SetHdr` 11w: **M1** — the target ranks the parameters
  above the locals (fhd r31 / sfh r30 / ver r29; result r30 / len r29 / p r28 / sfd r27), ours locals
  first (the pass-5 first-parameter copy lever fixes only the first one).
- mwsfdsvr `mwlSfdSleepDecSvr` 24w, 164/184: **M1** — the target materialises the zero for the inlined
  ClrSleepBdr twice as copies (`li r28, 0; mr r30, r28; mr r31, r28`), which needs 5 callee-saved
  registers and turns the prologue/epilogue into `stmw/lmw r27` (ours one `li` each, 4 registers,
  8 single stw/lwz); `mwsfd_ExecSvrHndl` 15w (mwply r31 / sfd r30 vs ours r30 / r31; with the
  `dont_inline` pragma the M3 inlining itself is fixed); `mwSfdExecDecSvrHndl` 12w (the pool `lis r4`
  above the `stw r0; stmw; mr r29` prologue stores, ours below them).
- sfd_tst `SFTST_Calc` 83w, 2640/2640: **M1** — the 64-bit abs diamond is kept in place with two `mr`
  in the target (`beq; subfic/subfze; b; mr r22, r25`), ours sinks the copies above the compare and
  renumbers r21..r25 through the rest of the function; `SFTST_Create` 4w: `lwz r3, sftst_debout_buf`
  + `cmplwi` scheduled above the header copy tail in ours, below it in the target.
- sfx_zmv `sfxzmv_MakeCnvZTbl` 94w, 1168/1168: **M1** — the inlined copy helper's src/dst are r3/r4 in
  the target and r4/r3 in ours (three inlined copies, plus r28/r29 in the fourth); `sfxzmv_MakeOrgZ32-
  TblByCCIR` 77w: the 8x unrolled 1.164f loop's slot/FPR order, as sfx_cnv (the .rodata order here
  is already the target's).

### CRI pass 9: mk-deception shapes (mpvabdec, mpv_cdec, dct_fsri, adx_dcd Matching; mwsfdply pin -> parameter; 2026-09-11)
Harness ~/.cache/cri9/ (deleted): `bytecmp.py lib/unit` = every section of our object vs the
split object (sizes, bytes with relocation fields masked, reloc offsets/types, symbol order; trailing
alignment padding tolerated), `bytecmp.py --funcs` = per-function byte identity, `trysrc.py unit file.c
[--noinc -i DIR]` = compile any source (e.g. the mk-deception unit) with the unit's flags against our
split object, `tryvar.py unit variants.py Func...` = `(old, new)` text replacements per variant. Reference:
github.com/ShulkMaster/mk-deception (the same CRI libraries one release earlier, GC/2.7). Their units
compiled against OUR split objects: every function the research pass listed is identical, every other
function is worse than ours (10 units screened: cri_cvfs 10/13, mpv_cmc 2/4, adx_baif 3/6, sfd_hds 8/11,
mwsfdsvr 9/12, sfd_tst 10/11, sfx_zmv 6/8, adx_stmc 16/19, adx_dcd5 1/4, adx_bau 4/7 — the same
functions fail there, so the pass-8 "M1" list has no missed semantics that their sources reveal).
- **mpvabdec 0/3 -> 3/3 (Matching): M5 was a source shape.** The bit reader is written out per look-ahead
  case: `prm->idx = zz[ofs]; bitpos += n; <store>; zz += ofs; if (bitpos >= 32) {bitpos -= 32; bbuf =
  nbuf << bitpos; nbuf = *ptr++;} else bbuf = bbuf << n;` (length added BEFORE the coefficient
  stores, refill after; `ofs` = run + 1 as a zigzag index, the second coefficient `zz[ofs2]` relative to
  the un-advanced pointer; the EOB cases do not advance `zz`). The long-code cases consume the shifted
  look-ahead INTO the sign (`code >>= 33 - len; code &= 1; prm->sign = code`), the escape reads
  `(Uint16)(bits >> 11) >> 2`, `(Sint8)((Uint32)level >> 8)` for the run, `hi = level * 2; level = hi |
  (Uint8)(bits >> 5)` for the 16-bit level, the 11..17-bit tables are `((const Sint16 *)tbl)[(idx & ~1U)
  >> 1]` with `idx & 1` the sign, `mpv->rl_8[idx]` is read twice (`rl = tbl[idx]; prm->run =
  (Uint8)tbl[idx]`). Shapes that MATTER: the escape case of NintraBlock/Dc11 shifts the look-ahead into
  a fresh value through a `static inline Uint32 mpvabdec_EscapeCode(Uint32 code) { return code << 1; }`
  (a block-local `Uint32 esc = code << 1` matches Nintra but renumbers Dc11's `packed` r29 -> r12;
  IntraBlock shifts `code` in place); the loop's `code`/`x` are block-local in Nintra/Intra (declared in
  the block around the loop) and function-level in Dc11 (declaring them in the loop too shadows and
  renumbers 3139 words); NintraBlock is `32 x blk[i] = 0.0` + three `static inline` helpers under
  `#pragma inline_max_size(100000)` / `inline_max_total_size(100000)`: `mpvabdec_NintraFirst(mpv, prm,
  bbuf0, nbuf, bitpos)` (the first-coefficient switch, `default:` = the rl_8 table), `bbuf =
  mpvabdec_NintraSkipFirst(bbuf0, prm->len, &bitpos, &nbuf, &ptr)` (out-parameters), `return
  mpvabdec_NintraAc(mpv, prm, bbuf, nbuf, bitpos, ptr)` (store + AC loop + save + result), themselves
  called from a fourth inline `mpvabdec_NintraDecode` that loads `bitpos, bbuf0, nbuf, ptr` in that order.
  Dc11 keeps `bbuf0` (the loaded window) separate from `bbuf` and refills from `bbuf0` after the DC
  read; IntraBlock's 16-bit DC peek is `dcv = bbuf >> 16; if (bitpos > 16) dcv |= nbuf >> (48 - bitpos)`
  with the sign fix `sbit = 1U << (dc - 1); if (!(dcv & sbit)) dcv += 1 - (Sint32)(sbit * 2); dc =
  (Sint32)dcv * 8`. Field types at the use sites: `(const Sint16 *)mpv->bitmsk_tbl`, `(const Float32 *)
  mpv->scale_tbl`, `(const Uint8 *)prm->dctbl`, `const Sint8 *zz`. Intra/non-intra differ only in
  `MPVABDEC_LEVEL(level)` (`level * 2` vs `level * 2 + 1`, `#undef`/redefine between the functions).
- **mwsfdply: the pass-6 r5 pin was a missed parameter.** `MWSFD_SetFlowLimit(mwply, (Sint32)(0.8 * n),
  n)` (min, max); the local `extern` in mwsfdply.c had two parameters. Lesson for the other "which
  argument register" residues: check the callee's real signature first (mwsfdset.c had the third
  parameter all along).
- **mpv_cdec MPVCDEC_IntraBlocks (Matching): the 192-store clear is six calls of `static inline void
  mpvcdec_ClearBlk(Float64 **cur)` with 32 `*(*cur)++ = 0.0;`** on a block-local `Float64 *cur =
  mpv->blk[0]` — the second base `addi r8, mpv, 0x720` and the store order follow from the cursor
  (loop, macro and per-block-pointer forms never produced it).
- **dct_fsri 5/9 -> 9/9 (Matching).** (a) `DCT_FsriTransCore`: the paired-single kernel's registers are
  `register __vec2x32float__` variables (six constants `c1..c6` loaded through a C pointer `p =
  B0TableOrg` with `psq_lu cN, 8(p)`, nine temporaries) and `src/dst/cnt/o` register locals — NO hard
  register in the asm. With hard `f0..f13/f31` and `r5/r7` in the asm our compiler poisons them for the
  whole function (the DC-fill path's `dc`/temporaries went to f29/f30 = two extra FPR saves, pa's fields
  to r8..r10); with variables the C paths share f0/f7 and r5/r7 with the kernel (target). `cnt` must
  be declared before `o` (else o r0 / cnt r9 swap). `__vec2x32float__` register variables are accepted
  by 2.4.7 as psq_l/ps_* operands (a `register` pointer is required for the base). The variable
  allocation order is first-definition order f0, f1.. for volatiles and the 15th value spills to f31
  (the first callee-saved), which is exactly the target's register set. (b) `dctfsri_Idx` is `static
  inline` with `int` arithmetic (`r *= 2` / `q--; r = r * 2 + 1; n = r + q * 8`); `initSparseTbl` under
  `#pragma opt_loop_invariants off` (the scan row is recomputed per store) storing through `static
  inline dctfsri_SetPreIdct(int n, int k, const Float64 *v)`; `DCT_FsriInitScanTbl(const Sint8 *seq, ..)`
  with an `int` counter and a `(Sint8)` cast. (c) `.data`: `B0TableOrg[0..1]` is sqrt(2)
  (1.4142135381698608f, not 1/sqrt(2)) and the literals need their full float digits
  (2.613126039505005f; `2.6131258f` rounds to a different word).
- **mwsfdcre `MWSFCRE_ResetSfdHn` (3 -> 4/10):** the picture-user-buffer block written out with the
  function's own locals declared ABOVE the handle (`pu, nskip, usize, buf, sfd` then `sfd = mwply->sfd`):
  the macro form (block-scoped locals after `void *sfd = ..`) ranks sfd r30 instead of r28. The other
  seven functions are worse in mk-deception too.
- **sfd_mpv 12 -> 15/38:** `sfmpv_ChkFatal` needed the missing `return` on the first `SFLIB_SetErr` (the
  `li r0, 0x80; cmpwi r0, 0x80` sizeof checks are kept at -O4, no optimization_level pragma) and
  `#pragma dont_inline on/off` around it (COMPILER-DIFF: M3 — deferred inlining inlines it into
  SFMPV_Init, the target calls it); `SFMPV_Init` loops forever (`for (;;) {}`) when ChkFatal fails and
  compares MPV_Init's result with **0xFF03FF05** (`addis r0, r3, 0xfd; cmplwi 0xff05`; ours had
  0xFF02FF05 and mk-deception 0xFFFDFF05 — read the constant off the `addis`), ternary error code;
  `SFD_SetMpvCond(NULL, id, val)` calls `MPV_SetCond(NULL, ..)` (sets the decoder default) instead of
  returning 0. Residues: `SFMPV_Stop` (target `lwz; li r3, 0; cmplwi r0, 0; blr` — a compare with no
  branch; `if (wk == NULL) return 0; return 0;` gives flag arithmetic, `ret = 0; if (..) ret = 0;
  return ret;` gives `cmplwi; bnelr; li; blr` (1w, kept), goto/break/switch/empty-if forms drop the
  compare), `SFD_CalcYccPlane` 5w (ywidth computed in place r5 vs a fresh r8, seven declaration orders
  tried, M1).
- **adx_dcd 9 -> 10/10 (Matching): `#pragma pool_data off` before `ADX_GetCoefficient`** (tagged M2;
  the unit has no .bss pool). The same pragma over-shoots everywhere else, measured: dct_ac
  `DCT_AcInit` 244 -> 264 bytes (loses the `dctac_i_const` .bss pool), sfd_adxt `SFADXT_SetSpeed` 42w
  -> 27w but 0xbc -> 0xc0 bytes, sfd_mpv `sfmpv_Pts2Tc` 0x1e4 -> 0x1f0 (global-table pool), sfx_cnv
  `SFX_MakeTable` 0x47c -> 0x47c but 194w and .rodata unchanged (its .bss pool). Those four stay M2.
- **adx_dcd5 (1/4): the "M5" was register ranking**, not shift forwarding: with the history locals
  declared first (`l1, l2, [r1, r2,] i, j, ...`) the `AdxQtbl` address goes straight into r28 after the
  `stmw` (target) instead of a hoisted `lis r11` above the `extsh`s; Mono4 54 -> 39w. The rest: the
  original keeps `smul` extended into r0 and `i` in r10 (volatile registers for locals of a leaf
  function) while ours extends smul in place and gives `i` a callee-saved register; c1/c2 in place
  (r9/r10) in the stereo decoders vs copies in ours (one more callee-saved). Operand order of the
  products (`AdxQtbl[d & 0xF] * sc`, `c2 * l1 + c1 * t`) does not change the code.
- **adx_baif `AIFF_GetInfo` 127w:** the target builds the FORM word and the size word 3 bytes in a
  temporary, copies (`mr r27, r30` / `mr r28, r12`) and merges byte 3 into the copy — the
  two-definition copy pattern; a stepping-`p` header (`ckid = LE32(p); p += 4; ...`, mk-deception's
  shape), `end = p + cksz - 4` (subi-then-add as the target, -1w, kept), static inline rd32/sw32 helpers
  (worse: 148w) do not produce the copies. M1.
- **M6 (adx_bau/adx_baif `ExecOne*16` copy 15):** mk-deception's loop shape (`left[i] = input[i*2] *
  256` indexed form) is 59 words off against our target; no shape information there.
- GC/2.7 made sfd_tst `SFTST_Create` identical (sfd_tst is 10/11, `SFTST_Calc` 83w M1 remains).
- Hazard: `ninja <target> -t clean` cleans EVERYTHING ninja built (all objects, RELs, build/tools binaries
  — `-t clean` ignores the target list); a full rebuild then exposed that wep00/34-37 did not compile at
  HEAD (`PSet` redefined in wep_mod.h, stale objects had masked it; fixed by another agent meanwhile).
  Never pass `-t clean`; delete the one object file instead.

### CRI pass 10 (mwsfdfrm, adx_bau, adx_stmc Matching; adx_tsvr 3 -> 4/6, all zero-pin C shapes; 2026-09-11)
Harness ~/.cache/cri10/ (deleted): `bytecmp.py lib/unit [--funcs]` (sections + per-function
bytes with relocation fields masked, NOBITS symbol order), `tryvar.py lib/unit variants.py Func.. [--offs
0x..,..] [--show]` = `(old, new)` text replacements per variant compiled with the unit's ninja flags, word
count per function and our-vs-target instruction at chosen offsets; ~60 ms per variant, so declaration
permutations of 5 locals (120) or two 4-variable groups (576) are cheap.
- **Declaration order CAN move the callee-saved ranking, but only in some functions.** mwsfdfrm
  `mwl_convFrmInfFromSFD` (10/11 -> Matching): the ten `x00..frmno` field copies declared LAST (after
  `bufadr`) take r31..r22 above the parameters (frm r21 / vfrm r20 / mwply r19) and the remaining locals
  follow in declaration order (time r18, ftype r17, pstruct r16, sfd r15, bufadr r14); declared before
  `bufadr` they sit below the parameters and `x00` (first declared) falls to r14. The tail needed `pptr`
  declared before `usrlen` and `usrptr` (pptr r15 / usrptr r17); 20 of 120 tail permutations work.
  The same sweep does NOTHING in adx_tsvr `ADXT_ExecHndl` (i vs the compiler IV, 72 orders), sfd_hds
  `sfhds_DoProcessHdr` (`ver` at 9 positions, 20 different names — names are irrelevant), sfd_cre
  `sfcre_AnalyAudio` (240 orders, q stays r24), gcci `gcCiReqRd` (576), adx_tsvr `adxt_stat_decinfo`
  (240): treat it as a cheap first sweep, not a rule.
- **A plain (non-`register`, no asm) copy of the parameter replaces two hard pins**: adx_tsvr
  `adxt_stat_decinfo` is byte-identical with `ADXT p; void *sjd; ... p = adxt; sjd = p->sjd;` (p declared
  first) where the pass-7 `asm { mr p, adxt }` + `asm { lwz r29, ..sjd(p); mr sjd, r29 }` pins left 3
  words (the r29 poison re-ranked sfreq/nch). The same plain copy does NOT replace the `mr p` pin in
  `adxt_nlp_trap_entry` / `adxt_trap_entry_lps` (45w/18w worse) nor help gcci `gcCiReqRd`, sfd_hds
  `SFHDS_SetHdr`, mwsfdsvr `mwsfd_ExecSvrHndl`: try it wherever a pin exists, keep the pin if it loses.
- **"M6" was a source shape**: adx_bau `ADXB_ExecOneAu16` / adx_baif `ADXB_ExecOneAiff16` (the unroller's
  15th copy `extrwi 8,16` vs `srawi 8`) are byte-identical with the swap through a `Uint16 x` temporary
  (`x = inbuf[i * 2]; out0[i] = (x << 8) | (x >> 8);` — with the temporary the operand order matters:
  `(x >> 8) | (x << 8)` gives the two rlwinm/rlwimi swapped in copy 15; `Uint32`/`Sint16`/`int`
  temporaries, `& 0xFF`, casts and `opt_unroll_count` change nothing or everything). adx_bau Matching;
  adx_baif 4 -> 5/6 (AIFF_GetInfo remains).
- **Derived-IV step in the latch** (adx_stmc `ADXSTM_Create`, Matching): `stm = (ADXSTM)((Uint8 *)
  adxstmf_obj + ofst * sizeof(ADXSTM_OBJ)); if (stm->used == 0) break; ofst++;` keeps the scaled index as
  the IV (`mulli` once, `add r31, base, ofs` per iteration, `addi ofs, 0x60` after the `beq`).
  `&adxstmf_obj[ofst++]` steps before the load; `&adxstmf_obj[ofst]` + `ofst++` after the test, the
  `Sint32`-cast sum, a byte-offset local and pointer stepping all become a pointer IV (`lbz 0(r3); mr
  r31, r3`, +8 bytes); `#pragma opt_strength_red off` changes nothing.
- gcci `gcCiReqRd`: `tbl = gcg_ci_obj; if (gcci_IsBusy(tbl)) ..; gcci_ExecServer(tbl)` is what makes the
  `mr r29, r5` copy (tbl in a volatile for the call-free IsBusy loop, copied for the inlined ExecServer's
  stepping parameter). With `gcci_IsBusy(void)` / `gcci_ExecServer(void)` both indexing `gcg_ci_obj[i]`
  directly the two `&gcg_ci_obj[0]` temporaries CSE into r29 like the target (155 -> 26w, the rest is the
  gcci r27 / buf r26 / nsct r25 parameter ranking) but gcCiExecServer then ranks i above the base (29w;
  the pointer-local-declared-last helper form gives gcCiExecServer identical and ReqRd the copy again).
  Also `gcci->sctlen * (over / gcci->sctlen)` for the target's `mullw r3, r4, r3` operand order. Not
  applied (no form fixes both callers). `gcCiClose`: a `GCCI gcci` copy declared LAST in gcCiStopTr gives
  the target's frame (stmw r24, 0x1c4 bytes, the second parameter copy r29) but tests the copy
  (`cmplwi r29, 0`) where the target reuses cr0 of Close's `mr.` (7w) and costs gcCiStopTr 6w.
- sfd_tst `SFTST_Calc`: `if (diff < 0) adiff = -diff; else adiff = diff;` (and the ternary) is
  forward-substituted into its single use after the sftst_Conv call (the diamond moves below the `bl`);
  a second use of `adiff` keeps it in place with the target's arm shape (`beq; subfic; subfze; b; mr`),
  but ours copies both words in the else arm (`mr r22, lo; mr r21, hi`, diff.hi in volatile r5) where
  the target coalesces adiff.hi with diff.hi (r23) and copies only the low word. Still M1 (82w).
- sfd_mpv `SFMPV_Stop` (`lwz; li r3, 0; cmplwi r0, 0; blr`): 33 forms (void/Bool/volatile compares,
  `?: 0 : 0`, `& 0`, `* 0`, if/else returns, do/while/switch/goto, `ret = ret`) give either the
  branch (`bnelr`) or no compare. OPEN.
- mpv_cmc `MPVCMC_InitMcOiRt/InitObj`: an inline helper taking `oi`, two-step pointer derivation,
  pointer stepping, `register`, `void *`, `Uint8 *` casts, block-local `wk = mpv->work` — the member
  array base is always folded into r3/r31 (OPEN since pass 1, 12 more forms).
- dct_ac `DCT_AcInit`: `#pragma pool_data off` plus explicit `dctac_i_const`-relative table pointers
  (`tbl[64 + ..]`, `(Uint8 *)base + 0x200/0x400`, `Float64 (*tbl)[8]`) trade the pooled literals for
  `stfdx`/`mr` forms (42..55w vs 58w); the target's `addi r28, r31, 0` is a pool-member address, so the
  function really has the .bss pool with unpooled FP literals (M2 exclusion rule, unchanged).
- sfh_main readers (`lwz r6` vs r5 for the swapped word): helper declaration orders, a `Uint32 *p`
  local, `register hdr`, indexed load, asm `lwz w, 0(p)` — the word always takes the dying base r5.
- adx_tsvr `adxt_nlp_trap_entry` (`lha r4` vs r0): local temporaries, `(Sint32)` casts, operand
  order, statement order — r0 in all 10 forms. `ADXT_ExecHndl`: the compiler IV of `adxt->sjo[i]` ranks
  above `i` (r28) in every declaration order / loop form; a source pointer gives `addi` instead of the
  folded `lwz 0x18(rIV)`.

### CRI pass 10b (sfd_tim 30 -> 34/39, sfd_mps 20 -> 21/26, sfd_buf 17 -> 18/26; pure C, no pins; 2026-09-11)
Harness ~/.cache/cri10b/ (deleted; cri10's `bytecmp.py --funcs` / `tryvar.py` copies). Nothing
flipped; every gain is a plain C shape:
- **Two-level inline structure for a public function inlined into another**: sfd_tim
  `SFTIM_IsGetFrmTimeTunit` has a DIRECT early return (`li r3, 1; b epilogue` for the `cond[14]` test) while
  every other exit goes through the result variable (`li r5; mr r3, r5`), and `SFTIM_IsGetFrmTime` has the
  same test through r5. That is: the `static inline` body helper WITHOUT the test, `Tunit = if (test) return
  TRUE; return helper(..)`, and `IsGetFrmTime = if (!frm) return FALSE; return SFTIM_IsGetFrmTimeTunit(sfd,
  frm->inf.raw[3], frm->inf.raw[4])` — the now-small public Tunit is auto-inlined (100% / 6w from 119w/124w).
  A big public body is never auto-inlined (`-inline all`, `inline_max_size`, `always_inline`; the
  `inline_max_auto_size` pragma is "illegal" in 2.4.7) and `inline` on a public function drops its out-of-line
  copy, so the two-level split is the only C form. The reload of `tim->vcnt` before `UTY_CmpTime` (pass 2
  blamed register pressure) is a real second read: `cnt = *(volatile Sint32 *)&tim->vcnt;` into the existing
  `cnt` local in the else block (the volatile read straight in the argument list ranks the value into r5 and
  copies `tunit` away; through the local it takes r0 / r5 like the target). Residue 6w: `tunit` r10 / `vrate`
  r9 swapped in the inlined copy only (wrapper locals, `register`, 11 helper declaration orders, arg order).
- **`frm = tc->frm + tc->frm2` as a local before the big product sum** (sftim_Tc2Time23N/29N/59N, 100%):
  the inline `(tc->frm + tc->frm2) * 1000` term computes `sec * rate` in place and the sum in fresh
  registers; the local makes the target's `mulli` into the freed `tc` register and the running sum in r4.
  Term order in the sum is already the target's (first addend added last); no other spelling moved it.
- **Result variable declared before the handle pointer** (sfd_buf `SFBUF_VfrmAddRead`, 100%): `Sint32 ret =
  0; SFBUF_HN *hn = ..;` gives ret the freed `n` register r4 and hn r6; hn-first gives the reverse.
- **Zero-copy loop counter = the clear loop in a `static` helper** (sfd_mps `SFMPS_Create`, 100%): the
  target's `li r5, 0; mr r6, r5` for `i = 0` is the inlined `sfmps_ClrOutSj(wk)` (`int i` loop over the 68
  `outsj` slots) copying the caller's NULL; `Sint32 i` in the helper loses the unroll shape (+8 bytes), and
  `i` must not remain declared in Create.
- Not moved (forms tried, exact class): sfd_tim `sftim_Tc2Time*D` 8w x3 (M1: target `addi r7, r7, 20756`
  const in place / `mullw r7` into the const register / `frm` r12 not the dying `tc` r4; mk-deception's
  one-expression `(frm + hour*.. + sec*24 + frm2) * 1000` gives the target's REGISTERS but flattens the add
  chain — hour term added last overall — and every split (`f = chain; f += frm2; f = frm + f`, parentheses,
  casts, inline helper for the chain, 20 forms) either keeps the chain order with our registers or the
  registers with the flat chain), `SFTIM_IsStagnant` 1w (else arm `lwz ext_cnt` before `lwz chg_base`: the
  one-expression `a - b` loads the right operand first, the two-statement form loads left first but turns
  `d` into an r3 variable; 25 forms), `SFTIM_IsGetFrmTime` 6w (above). adx_sje `adxsje_output_header` 1w
  (`li r5, 1` argument scheduled one slot earlier around the branchless `(key == 0) ? 0 : 8`; 9 spellings
  incl. `if` forms which add branches), `adxsje_write_end_code` 1w (inlined `adxsje_put16` loads `ck.data`
  before `*(Sint16 *)src`; pointer local, `Uint16`, index, `memcpy` (a call), `Sint16 *` parameter — all
  keep the value load first). mps_lib `mpslib_SetErr(NULL, ..)` inline site (5 functions): target `lwz r5,
  MPSLIB_libwork@l(r4); addi r4, r3, 0x103` (lw in a fresh register, the code constant in the dying `lis`
  register) vs ours `addi r4, r4, @l; lwz r4, 0(r4); addi r0` — 14 helper forms (direct global access,
  split helpers, two-definition `lw`/`code`, `register`, volatile global, `Uint32` code, `MPSLIB_WORK **`)
  are byte-identical to each other; M1. sfd_buf `SFBUF_RingGetDataSiz` 4w (`mr r5, r4` zero copy for
  `len2 = 0`: inline zero helper, len-first declarations, the body as an inline helper with out-params —
  none copies). sfd_mps `sfmps_ProcPrep` 7w (`shdr` of the inlined `sfmps_GetSeeShdr` in r5 above the
  `see.wk` r4, ours in place; helper result-variable forms, `w` before/after). mpv_umc `MPVUMC_BiDirect` 2w
  (`addi r4, &ccnt_rt` scheduled before `lwz ofs[0]; mr r3, wk`; local ref/cbp, direct `&mpv->mcwk`).
- mk-deception shapes for these units: sfd_buf/sfd_tim/mps_lib/adx_sje/sfd_mps/mpv_umc/mpv_mcy/adx_bsc/
  cft* sources exist there (decompiled style, `SfdHandle` naming); only the Tc2Time*D expression was
  informative (see above).

### CRI pass 11: MWCC register-ranking model (adx_tsvr 4 -> 5/6, mpv_hdec 12 -> 13/15 + .text order; 2026-09-11)
Harness ~/.cache/cri11/ (deleted): cri10's `bytecmp.py`/`tryvar.py`, `mwcc.sh file.c` (CRI-flag
compile + objdump of a probe), and **`ra.py lib/unit Func [--src file.c]`** = cadmic's mwcc-debugger
(github.com/cadmic/mwcc-debugger, run under encounter's retrowin32 `gdb-stub` branch, `cargo build -p
retrowin32 -F x86-unicorn --profile lto` with `RUSTFLAGS="-C link-arg=-latomic"`; ~3 s per function) on the
GC/2.6 compiler (2.4.7 build 107, same codegen as our GC/2.7 build 108): it dumps the frontend AST, the
backend PCode before/after every pass and the GPR/FPR interference graph with the priority list
(`regalloc-gpr-pass-1-assigned.txt`: virtual id, assigned register, variable name, degree at removal and
total degree; `-all.txt` adds the neighbour lists and the coalesced aliases). Rebuild it whenever a ranking
question comes up: reading the dump replaces permutation brute force.

**The allocator (Chaitin colouring, verified on ~40 probes and the open CRI functions):**
- Every value is a virtual register >= r32. **Ids: the parameters first in order (a=r32, b=r33, ...), then
  the function's own locals in REVERSE declaration order (last declared = lowest id, first declared =
  highest), then the locals/parameters of each inlined helper in declaration order (parameter first), then
  the frontend's named temporaries (`@N`; range-split copies of a variable, hoisted constants, CSE values,
  strength-reduced induction pointers; created first = highest id) and the backend's temporaries in
  creation order.** Address-taken/aggregate locals have no id (stack). A variable the frontend
  copy-propagates away (`p = param` then only `p` used; a call result used once; `t = a; f(t)`) leaves a
  dead id: the value that survives is the temporary, so a call result stored or used in arithmetic ranks as
  a temporary (later call = higher), while a call result whose copy bounces (`mr r0,r3; <arg move>; mr
  r31,r0`) keeps the variable's id and ranks by declaration.
- **Priority list:** scan ids upward, remove every node whose remaining degree is < the free-colour count
  (29 GPRs: r1/r2/r13 reserved), push it at the HEAD of the list, decrement its neighbours; repeat the scan
  until nothing is removable, then remove the cheapest spill candidate and continue. So the list is a
  series of levels: nodes removed in a later iteration are coloured before every node of an earlier one,
  and inside a level the highest id is coloured first. A value live across a call has the 12 physical
  neighbours r0, r1, r3..r12 plus every overlapping value; values coalesced away (`mr` copies, argument
  moves) stay as neighbours ("ghosts") and inflate the degrees.
- **Colouring:** highest priority first; a node takes the lowest-numbered free register among r0, r3..r12
  and the callee-saved registers ALREADY handed out in this function, else a NEW callee-saved register
  starting at r31 downward. A temporary is therefore "in place" (reuses its dying operand) exactly when
  that operand's register is the lowest free one; a second block of a function reuses the first block's
  callee-saved registers from the lowest up (adx_tsvr `ADXT_ExecHndl`: loop 2 gets r27, r28, r29 in
  colouring order, then a new r26).
- Consequences that were previously called "M1": parameters rank below every local of the same level
  (first parameter lowest); the locals of a single level rank by declaration order (first declared highest)
  — the reverse of pass 10's reading; a frontend temporary outranks every variable of its level (a
  hoisted `len - 3`, a range-split second definition, an induction pointer); a node with >= 29 neighbours
  (live across the whole function next to ~17 other values, or fewer with ghosts) jumps to the next level
  and lands above everything else (mwsfdfrm's ten copies, sfd_hds `id`, mpv_hdec `n`).

**Fixed this pass (pure C):** adx_tsvr `ADXT_ExecHndl` (the two counted loops step a `Uint8 *p`/`p2` copy
of `adxt` by 4 and read `*(SJ *)(p + 0x18)` — a source pointer initialised from the object gives the
target's `mr rIV, r31; lwz 0x18(rIV); addi rIV, 4` where a pointer to `adxt->sjo` gives `addi` — with the
declaration order `nch, i, p, j, sj, nbyte, sjd, rna, ndata, nroom, p2, ck`: loop-1 values before loop-2
values, each counter before its pointer, `sj` before `nbyte`; a second counter `j` because a reused `i` is
range-split into a temporary that outranks the loop-1 values). mpv_hdec `mpvhdec_AnalyUd` (`for (i = 4; i <
len - 3; i++)` and one `n = i` — the pre-loop `n = len - 3` made `n` a two-definition variable whose
range-split copy jumped to r31 above type/ret/ret2), `mpvhdec_DecPscSj` 3 -> 2w (`dc11 = (cond[6] == 3) ? 0 :
1` numbers the `dc11*20`/`type*4` index temporaries like the target; `!= 3` swaps them), and the .text order:
`MPV_GoNextDelimSj` is a `static mpvhdec_GoNextDelim` (inlined into NextDelim/DecPicture) plus the public
function with the SAME body defined after `MPVHDEC_DecPicture` (a wrapper `return helper(sj)` lays the
frame out as an inlined block and bounces `delim`, -4 bytes).

**Residues read off the dumps (what the target's graph must have had):**
- sfh_main readers (`lwz r6` vs the dying base r5 / `r4` in SmpHz): the swapped word's node has exactly one
  more coloured neighbour in the target than in ours (r5 = `hdr` for the six helper readers, r4 = `id` in
  SmpHz) — the original build's peephole merged the rlwinm/or chain AFTER register allocation, leaving one
  partial-result temporary live and coloured; ours merges before allocation (and folds the store to
  `stwbrx` in the post-allocation peephole = M4). Keeping `hdr` live (`return hdr != NULL`) moves the word
  to r6 but costs code; same root as M4, 1 word each, accepted.
- sfd_hds `SFHDS_SetHdr`: `result` has 28 neighbours in ours (one level with p/len -> p r30, len r29,
  result r28); the target's order (result r30, len r29, p r28) needs `result` at >= 29, i.e. one more ghost.
  `sfhds_DoProcessHdr`: the range-split vid `id` has 30 neighbours (level 2 -> r31 above fhd/sfh); the
  target has it in level 1 (r29 below fhd r31 / sfh r30), i.e. <= 28 — dropping the `neg/or/rlwinm` chain
  of `ftr_eff = (eff != 0)` (`(Bool)eff`) gives the target's registers but loses the chain. Nine ternary
  temporaries, the picw/pich -1 and the eff loads are its neighbours; no spelling of them (`?:`, `!!`,
  if/else, a local) changes the count.
- mpv_hdec `MPV_DecodePicAtrSj`: target `mpv r28 / sj r27` = mpv removed one iteration LATER than sj (its
  remaining degree stayed >= 29 while sj's dropped); ours removes both in the same iteration (25/24 left).
  Frame: the nested inlined GoNextDelim/MoveChunk aggregates (NextDelim inside PicAtr) are allocated after
  the direct inlined copies in ours (second inlining round = lowest offsets, see t7 below) and before them
  in the target; NextDelim as a macro changes the control flow. `mpvhdec_DecPscSj` 2w: the GET value of
  `r_size` and `r_size--` are one node in the target (`addi r9, r9, -1`), two in ours (the frontend
  range-splits the decrement whatever the spelling: `--`, `-= 1`, `= x - 1`, `register`, `Uint32`, via
  `val`).
- gcci `gcCiReqRd` (gcci r27 above buf r26 / nsct r25, with the callee-saved temporaries above it): the
  target removed nsct/buf in iteration 2 and gcci + the four temporaries in iteration 3; ours removes all
  seven in iteration 2 (remaining degrees 13..25). `gcCiExecServer` (gcci r30 / i r29): the inlined
  helper's parameter (lowest inlined id) below its local `i`; the target has the pointer above `i`.
  mwsfdsvr `mwsfd_ExecSvrHndl` (mwply r31 / sfd r30): 21/20 neighbours, one level, param below local in
  ours; a plain `p = mwply` copy is propagated away (no node), only the asm copy survives. These are the
  same signature as pass 5's "asm copy of the first parameter" lever: the target's IR kept copies (ghost
  neighbours) that our frontend propagates away, pushing the parameter into a later level.
- adx_tsvr `adxt_nlp_trap_entry` (`lha r4` vs `r0`): the load temporary's neighbours are all coloured
  r3/r26..r31 in ours (r0 lowest free); the target has an r0-coloured neighbour we do not have.
- Frame layout (probe t6/t7): own aggregates first-declared highest, then block-scoped own aggregates,
  then inlined helpers' aggregates in inlining order (nested/second-round inlines lowest), independent of
  code order.

### CRI pass 12: the kept parameter copy (mwsfdsvr Matching; gcci 12 -> 13/15; adx_tsvr pins -> C; 2026-09-11)
Tooling (PERSISTENT, reuse it): `tools/research/mwccdbg/` = cadmic's mwcc-debugger + encounter's
retrowin32 `gdb-stub` build; `README.md` there has the exact build/run commands; `ra.py lib/unit Func
[--src file.c] [--out DIR]` runs it with the unit's ninja flags on GC/2.6 (~3 s, static functions by plain
name) and `rasum.py DIR [--nb]` prints the priority list as one line per node (`vid -> reg name
removed-degree/total [neighbours]` + the coalesced ghosts). Pass harness ~/.cache/cri12/
(deleted): `bytecmp.py lib/unit [--funcs]`, `tryvar.py lib/unit variants.py Func.. [--offs] [--keep LABEL]`
(~60 ms per variant, `--keep` writes the variant source for ra.py), probe files `probe_copy*.c`.

**The "kept copy" question, answered with probes (t1..t7, u1..u16, v1..v12 in probe_copy*.c):**
- A plain `p = param` is removed by the FRONTEND (copy propagation). A copy whose right side carries a
  type conversion is kept by the frontend as `EASS p = ETYPCON(param)`: `void *obj` -> `MWPLY mwply =
  obj` (implicit), `(MWPLY)obj`, and even a no-op explicit cast `(MWPLY)(MWPLY_OBJ *)mwply` or
  `(Sint32 *)(void *)result`. Conversions of call ARGUMENTS (`f((S *)p)`, `void *` prototypes) create
  no temporaries and no ghosts (probe w1..w5).
- The BACKEND then propagates that `mr copy, param` away unless the copy is later moved into an
  argument register with `mr rN, copy` that cannot collapse: i.e. rN != the parameter's own register
  (u14 `f2(0, p)` with p from r3; u2 `f(p)` with p from r4) or rN == the parameter's register but only
  after a call clobbered it (v10 `f(p->sfd); f(p)`). `f(p)` as the FIRST call of an r3 parameter (u4,
  u16 with r4/r4), stores of the copy (`glob = p`), loads/stores through it, compares (u3, u5, u13, v1..v7)
  all let the backend remove the copy. When it survives, the parameter is coalesced into its argument
  register (ghost `r32 -> r3 = obj`) and the copy is a real node with the LOCAL's id: it ranks by its
  declaration position (last declared = lowest), above the parameters — that is the pass-5 "asm copy of
  the first parameter" lever and pass 11's "ghost the original kept", in plain C.
- Where the original CRI code got such copies for free: handlers declared with `void *` parameters
  (server callbacks, CVFS interface functions, decoder trap callbacks) that convert the object to the
  typed handle in their first statement. adx_sjd's real `ADXSJD_EntryTrapFunc(sjd, void (*fn)(void *obj),
  obj)` signature confirms the callback type.

**Fixed (pure C, no pins):**
- mwsfdsvr (Matching). `mwsfd_ExecSvrHndl(void *obj)` + `MWPLY mwply = obj` declared before `sfd` (mwply
  r31 above sfd r30; the M3 `dont_inline` pragma stays). `mwSfdExecDecSvrHndl(void *obj)`: the kept copy
  also schedules the pool `lis` above the parameter move (target `lis r4; stw r0; stmw; mr r29, r3`),
  and ONE function-scope `void *sfd` assigned in both switch cases: its PLAYING redefinition is a
  range-split frontend copy that outranks the backend temporaries (r28, where a block-scoped second
  `sfd` got r27); `sfd` declared before `mwply` (mwply r29 below sfd r30). `mwlSfdSleepDecSvr`: the 10x
  wait loop as its own `static void mwsfd_SleepLoop(MWPLY)` helper (inlined) — the two zero stores of
  the inlined ClrSleepBdr then become copies of the counter's zero (`li r28, 0; li r29, 1; mr r30, r28;
  mr r31, r28`) because the helper's `i = 0` lands in the same block as the hoisted loop constants and
  the backend CSE pairs each `li 0` with the first one (i is multi-def, so the copies cannot be
  propagated; the two `li 1` collapse into one). In the caller's own loop the `li i, 0` sits in the
  `if` block and the constants in a new preheader block: CSE is block-local (do/while, while, `for`
  without init, `Sint32 i = 0` at the top, register/short/Uint8 counters: all 43w).
- gcci 12 -> 13/15: the CVFS handlers take `void *hn` (`GCCI gcci = hn`, as the `CVFS_IF` slots are
  typed): `gcCiReqRd` gets the target's gcci r27 / buf r26 / nsct r25 (157 -> 1w together with
  `gcci_IsBusy(gcg_ci_obj)`/`gcci_ExecServer(gcg_ci_obj)` indexing `tbl[i]`, no `tbl` local), `gcCiClose`
  (Matching) needs `gcCiStopTr(void *hn)` as well and the call written `gcCiStopTr(hn)`: the inlined
  copy is then a second copy of r3 (`mr. r28, r3; mr r29, r3`, stmw r24) instead of a copy of gcci.
- adx_tsvr: the three trap callbacks `adxt_trap_entry_lps/adxt_nlp_trap_entry/adxt_trap_entry(void *obj)`
  with `ADXT p = obj` replace the two pass-5 `asm { mr p, adxt }` pins byte for byte (still 5/6:
  nlp_trap_entry's `lha r4` below).

**Residues read off the dumps (exact class):**
- gcci `gcCiReqRd` 1w: the target initialises the ExecServer stepping pointer `addi r29, pool, 0x14`
  BEFORE the inlined IsBusy loop and IsBusy's own pointer is `mr r4, r29`; ours materialises IsBusy's
  pointer directly and ExecServer's at its loop. A `tbl` local stepped by an inlined `gcci++` parameter
  gives `mr r29, r5` (the copy of a multi-definition destination is neither propagated nor coalesced);
  a macro loop stepping `tbl` itself gives the target's copy structure but ranks the loop locals as
  own locals (153w). `gcCiExecServer` 33w wants the pointer above `i` (pointer-local-declared-last
  helper: 0w there, 94w in ReqRd) — no single helper body fits both callers (18 forms).
- sfd_hds `sfhds_DoProcessHdr`: the vid-section `id` is the range-split copy @151 (30 neighbours = 12
  physical + sfh/fhd + the NINE `?:` result temporaries @122..@146 (created at IR conversion, so they
  get HIGHER ids than the split copies made later) + `li -1`, `li 0`, the `eff != 0` chain and the
  second `lwz eff`). Removal scans ids upward: a node is removed in iteration 1 only if its degree is
  < 29 at its own scan, and @151's small neighbours all have higher ids, so it survives to iteration
  2 with sfh/fhd and is pushed after them (r31). The target needs @151 removed in iteration 1: two
  neighbours with lower ids or absent. Block-scoped `Sint32 val` for the ternaries, a shared `t`, a
  separate `vid_id` local (still 30, still level 2 -> r31), if/else stores (two stw) do not do it.
  `SFHDS_SetHdr`: target order result r30 / len r29 / p r28 = `result` a node between len and the
  frontend temps, i.e. an inlined-helper parameter or first-declared local copy; casts on the call
  argument and `(Sint32 *)(void *)result` copies are propagated (no argument move exists for it), a
  wrapper+helper split turns the Bool return into an r7 variable (44w).
- adx_tsvr `adxt_nlp_trap_entry` `lha r4` vs `r0`: the temporary's neighbours are r1, r3 (n2 in r3 via
  the coalesced `?:` copies r54/r55), ofst1, n1, sfd/sji/p, ofst2v; no r0-coloured node exists in the
  target's instruction stream between the join and the add, `add` operands get no physical-r0 edge
  (only `addi`/load bases do, cf. r44/r46 vs r51), so the extra edge is not derivable from the bytes;
  31 spellings (ternary/if forms of n2, initialisations, casts, orders, Sint16 temporaries) leave r0.
- cri_cvfs `cvFsAddDev` 32w: level 2 = {pool@, errfn@, vtbl (30), devname (40)}, coloured by id: vtbl
  r29 above devname r28; the target has devname r29 / vtbl r28 and the direct `mr r28, r3` of a
  redefinition (`vtbl = NULL` inits are dead-store-eliminated, 5 forms).
- mpv_hdec `MPV_DecodePicAtrSj`: mpv/sj swap is the kept-copy class (mpv passed to the inlined helpers'
  real calls after calls) but the frame-layout residue (0x20 shift, pass 7) stays; not touched.

### CRI pass 12b: reading the ids off the dumps (mps_lib 2 -> 5/7, sfd_tim 34 -> 37/39 + .rodata, sfd_buf 18 -> 21/26; pure C; 2026-09-11)
Harness ~/.cache/cri12b/ (deleted; cri12's bytecmp/tryvar + `cc.sh lib/unit` = the unit's exact
ninja command (compile + strip_unused) run directly, because `ninja <obj>` regenerates build.ninja and races
with other agents' configure runs — a truncated objdiff.json fails configure mid-read). tools/research/mwccdbg
reused. Every fix below was predicted from the pass-11 model plus one dump, then confirmed in 1-3 builds.

**Model additions (read off the dumps, verified):**
- The physical `r0` in a node's neighbour list is the rA/base constraint (`addi`, load/store bases), not an
  interference: `stw` sources, `cmpi` operands, `mr`/`mtctr` sources may take r0. So a value that ends in
  r4 where ours has r0 either is an addressing base, an argument (precoloured), or has an r0-coloured
  neighbour — check which before looking for a ranking cause.
- **Backend temporaries are created right operand first**: for `a + b` / `a - b` / `*p = *q` the right
  operand's temps get the LOWER ids (coloured later, higher registers). A load placed as the right operand
  of the top-level `+` is created before every product temp of the left operand (`f = chain + (tc->frm +
  tc->frm2)`), while the frontend's reassociation still adds the left chain's first term last.
- **The backend CSE turns a later `li rX, K` into `mr rX, rFirst` only when rX is a temporary**: a named
  variable's `li` is never rewritten (MPS_Init `Sint32 i` keeps `li r7, 0` next to the stores' `li r0, 0`;
  the same loop in an inlined `static` helper, whose locals are @temps, shares r5). Inlined-helper locals
  are @temps numbered in DECLARATION order downward (first declared = highest @N = lowest id), and their
  initialisers are emitted in statement order, so the copy DIRECTION follows the assignment order and the
  REGISTER the reverse declaration order (sfd_buf `len1 = 0; len2 = 0;` with `len2` declared before `len1`
  = target `li r4, 0` (len1) / `mr r5, r4` (len2)).
- **A backend constant materialised into an argument register is `addi rArg, rHi, lo` with the `lis` in a
  different register**: mps_lib's `lis r3, 0xff02; addi r4, r3, 0x103` was the code passed as the
  callback's SECOND argument (`errfn(errobj, code)`), live into the `bctrl`; that argument node takes r4
  and pushes the libwork pointer to r5, which is what lets the post-RA peephole fold `addi r4, r4, @l; lwz
  r5, 0(r4)` into `lwz r5, MPSLIB_libwork@l(r4)` (it folds only when the load's destination differs from
  the base — the standalone `MPS_SetErrFn`/`MPSLIB_SetErr` bodies pass one argument and keep
  `addi/lwz r3, 0(r3)` in both builds). Pass 9's lesson again: read the callee's real arity off the
  argument registers that are live into the call.
- **Literal .rodata order is @N creation order, not first-use order**, and an int -> float conversion's
  0x43300000_80000000 constant is created when the function that contains it is lowered (after its parse),
  while a float literal is created at parse. A dead `static Float32 f(Sint32 v) { return (Float32)v; }`
  placed BEFORE the first function with a float literal creates the double first (sfd_tim: [pad][double]
  [10000.0f][-1.0f], .rodata 0x80 identical; strip_unused removes the function). The target's @N counter
  runs 3-5x ahead of ours in sfd_tim (@474/@518/@1142 vs @93/@95/@548): the original TU parsed a lot more
  frontend material (header inlines), which is where such early constants come from.
- A parameter used before AND after a call is split into the parameter register for the early uses plus a
  `mr r31, rParam` copy later (ours) unless a value computed from the OTHER parameters is younger than the
  early uses: sfd_buf `hn = SFBUF_GET_HN(sfd, n)` assigned AFTER the seven `inf->` clears gives the
  target's `mr r31, r5; li r5, 0; mulli r0; add r3, r3, r0` (product r0 = younger than the zero, inf in
  r31 from the top; the declaration position of `hn` is irrelevant).
- Post-RA scheduling ties (MPS_Create `li r4, -1` / `addi r0, r3, @l`, mpv_umc `MPVUMC_BiDirect` `addi r4`
  / `mr r3, r29` argument hoists, adx_sje `adxsje_output_header` `li r5, 1`): the pre-RA order is the
  target's in ours and the post-RA list scheduler swaps the pair; the same scheduler on the same DAG
  cannot differ, so the target's DAG has an edge ours lacks — 20 statement/local/loop forms did not
  create it. Treat 1-2w "one slot earlier" residues as this class and stop early.

**Fixed:** mps_lib `MPS_Destroy`/`MPS_SetErrFn`/`MPS_Finish` (the 2-argument `mpslib_SetLibErr(code)`
helper through a local `MPSLIB_ERRFN2` pointer type; the mps.h field stays 1-argument for the public
bodies), sfd_tim `sftim_Tc2Time59D/29D/23D` (`f = chain + (tc->frm + tc->frm2)`), sfd_tim .rodata (dead
`sftim_Sint32ToFloat32`), sfd_buf `SFBUF_RingGetDataSiz` (`static sfbuf_RingGetDataSizHn(hn)` body helper,
`len2`/`len1` declared in that order and assigned `len1 = 0; len2 = 0;`), `SFBUF_RingGetRead/Write`
(`hn` computed after the `inf` clear).

**Residues (exact class):**
- mps_lib `MPS_Init` 48w: target `li r5, 0` shared by the three libwork clears AND the loop counter with
  the guard folded to `cmpwi r31, 0` (the frontend saw `i = 0` as a variable, the backend saw a temporary
  — a later web of a range-split variable: `i = *(Uint8 *)&test_wrok` first gives both but colours that
  first web r8 where the target's byte compare is an r0 temporary with `cmplwi`), plus an unreachable
  `b .L638; b .L644` pair after the remainder loop (a `return` with r3 untouched laid out between the
  loop and the calls; `for(;;)`/`while(1)` + `return` are deleted by the frontend, `if (x) return;` after
  the loop keeps the compare; 25 forms). `MPS_Create` 2w: the post-RA swap above.
- sfd_tim `SFTIM_IsGetFrmTime` 6w: the target's `tunit` node has an id BETWEEN the inlined helper's
  `vrate` (@116) and `tscale` (@118), i.e. it is a helper-level value declared where `adj` is, not the
  call-site argument temporary (@107, ours) nor an own local (r12: coloured after tscale/ncount); `unit =
  tunit` copies in the helper are propagated, `direct inner call`/`ret` forms lose the Tunit shape.
  `SFTIM_IsStagnant` 2w: else arm `lwz ext_cnt` first AND in r0 = the left operand created first yet
  coloured before `chg_base` (so `chg_base` must be r0-blocked or carry an r0 neighbour there); two-def
  `d = ext_cnt; d -= chg_base` gives the order with d in r3 (14 forms).
- sfd_buf `SFBUF_RingAddWrite/AddRead`: target keeps `ring = &hn->w.u.ring` (`addi r28, r3, 0x1318`) as a
  callee-saved node with `hn` dying in r3 before the calls, `nbyte` copied with `mr. r31, r5` (highest id
  = a temporary, not the parameter) and `bne body; b end` for the `nbyte == 0` exit; `sj = ring->sup.sj`
  (two uses) is still folded through `hn`. `SetSupplySj`/`DestroySj`/`InitHn` (union alias) untouched.
- adx_sje `adxsje_write_end_code` 1w: the inlined put16 site 1 has `lwz ck.data` before `lha v` while the
  pre-RA IR (ours) has the value load first in both sites; site 2's `lha` is forwarded to `extsh r0, r31`
  by the post-RA peephole (its `sth r31` survives the call) and then scheduled after the address load —
  so the target's site 1 behaves like a non-load: 6 spellings of the store keep `lha` first.
- mpv_umc `MPVUMC_BiDirect` 2w: post-RA tie (above); `Forward/Backward/Intra/OneReadMb` untouched.
- Build hazard this pass: with several agents running `configure.py`, `ninja <target>` fails on a
  half-written objdiff.json and `main.dol: FAILED` appeared from another agent's DOL flip (game/
  cam_qfps.cpp) while all three units here are still `False`; use the unit's exact compile command
  (`ninja -t commands <obj>`) when the manifest regeneration is racing.

### CRI pass 13: two-level helpers and per-site locals (gcci Matching; mpv_hdec 13 -> 14/15, sfd_hds 9 -> 10/11, cri_cvfs 10 -> 11/13; pure C, no pins; 2026-09-11)
Harness ~/.cache/cri13/ (deleted): cri12b's `bytecmp.py`/`tryvar.py`, `fd.py lib/unit Func [--src
file.c] [--all]` (side-by-side target/ours via `dtk elf disasm`, labels and reloc symbols normalised, no
ninja), `bld.sh lib/unit` (the unit's exact ninja command + bytecmp; `ninja <obj>` regenerated build.ninja in
a loop and failed on other agents' half-written objdiff.json every time this pass). tools/research/mwccdbg reused;
every fix was predicted from one dump before the build.

**Fixed (model prediction -> result):**
- gcci `gcCiReqRd` 1w -> 0: `tbl[i].sctlen * (over / tbl[i].sctlen)` (target `mullw r3, r4, r3`; pass 10's
  note). `gcCiExecServer` 33w -> 0 in two steps read off the ids: (1) the counter `i` must rank BELOW the
  frontend's induction pointer @433 in ExecServer but ABOVE it in ReqRd (i r30 / pointer r29 there) — an
  inlined helper's local ranks above the later strength-reduction temp, an OWN local below it, so
  gcCiExecServer got its own loop (10w); (2) the CANCELED `over = DVDGetTransferredSize()` is a range-split
  copy @439 that outranks the own locals (r28) while the target colours it after `over`/`p`/`nbyte` (r27):
  the loop body became a per-handle `static inline gcci_ExecOne(GCCI ci)` with `over, nbyte, p` declared in
  that order — inlined locals are created at inlining, i.e. BEFORE the split copy, so the copy ranks below
  them and takes over's r27 in both callers; a separate CANCELED variable bounces (`mr r0, r3`, +4). The
  table loop is written twice (`gcci_ExecServer(tbl)` for ReqRd, gcCiExecServer's own `for`) = the
  parent's "two helpers" guess. Case order in the switch is code order (swapping CANCELED/END: 66w).
- mpv_hdec `mpvhdec_DecPscSj` 8w -> 0: `mpv->fwd.r_size = --r_size;` — a statement `r_size--` after the
  3-arm GET join is forward-substituted as `r_size + -1` into its three uses (backend CSE temp r9 next to
  the raw value r7); the pre-decrement inside the store expression stays an in-place update (`srwi r9;
  subi r9, r9, 1`). `--r_size;`, `-= 1`, `+= -1`, a 4th use, `x = r_size - 1` forms all substitute.
- mpv_hdec `MPV_DecodePicAtrSj` 226w -> 21 rows (0x74c size): (a) frame: inlined aggregates are laid out by
  inlining ROUND (round 1 = calls in the function body, round 2 = calls inside inlined bodies; within a
  round in @ order), own aggregates above all; the EXT/UD case bodies as `static inline mpvhdec_SkipExt/
  SkipUd(mpv, sj)` (GetChunk + SETPOS + skip + `MPV_GoNextDelimSj`) make their `rest` round-1 objects and
  their GoNextDelim chunks round-2 ones AFTER NextDelim's nested GoNextDelim/MoveChunk = target order
  [ck, ck2][rest, rest][NextDelim's ck/rest/ck][EXT's][UD's]; (b) `MPVBIT_BYTEPTR(q); q += 12;` (its -8
  plus the 4 start-code bytes) keeps the target's `(ptr + n) + 4` association — `(Uint8 *)ptr + ((bitpos +
  7) >> 3) + 4` and the two-statement/parenthesised forms are reassociated to `ptr + (n + 4)`; (c) `MPV mpv
  = (MPV)(MPV_OBJ *)hn` (pass 12's kept-copy prediction): mpv r28 above sj r27, the `mr r3, r28` argument
  moves after calls keep it. Helper locals `bitpos, ptr, q, rest` in that order.
- sfd_hds `sfhds_DoProcessHdr` 111w -> 0 (the parent's reading, exactly): the vid-section `fhd->vid.x =
  (SFH_Anly..(sfh, id, &v) == 0) ? -1 : v;` sites rewritten `if (..) v_x = -1; else v_x = v; fhd->vid.x =
  v_x;` with a DISTINCT own local per site (10 locals; identical code). Each `?:` join value was a frontend
  temporary with a higher id than the range-split `id` copy @151 (scanned after it, so @151 met 30 live
  neighbours and stayed for iteration 2 with fhd/sfh -> r31); as own locals they are scanned and removed
  first, @151 drops below 29 in iteration 1 and colours with ver (r29), fhd r31 / sfh r30. All 25 sites
  converted also give 0w; a shared `t` is range-split into temps again (pass 12).
- cri_cvfs `cvFsAddDev` 32w -> 0 in three kept-copy/helper steps: `name = (Char8 *)(void *)devname` (argument
  moves after calls keep it; declared first: devname r29 above the table entry r28, 32 -> 28w); the search
  + registration as `static CVFS_DEVIF *cvfs_AddDevTbl(Char8 *devname, void *vt)` with `CVFS_DEVIF *vtbl =
  vt` and `return vtbl` on the found path, called `vtbl = cvfs_AddDevTbl(name, getif())`: the `return` on
  the found path is the `beq add; b check` exit and the typed copy of the `void *` parameter takes getif's
  result directly (`mr r28, r3`; `vtbl = getif()` in the caller bounces through r0 whatever the helper,
  +4 bytes) (28 -> 5w); `fn = (CVFS_GETIFFN)(void *)getif` orders the prologue's two pool `lis` (rodata
  `lis r5; addi r30, r5` then the bss `lis r5` — with the plain parameter both `lis` are hoisted together
  into r6/r5) (5 -> 0w). Helper locals `dev, i` (i r3 / dev r4 in the unrolled scan).

**Residues (exact class, forms tried):**
- adx_tsvr `adxt_nlp_trap_entry` `lha r4` (2w, 5/6 stays): T = @56 with neighbours r1, r3 (n2 = the
  coalesced `?:` copies), ofst1, n1, sfd/sji/p, ofst2v; it is the RB operand of `add` (no r0 base
  constraint), nothing r0-coloured is live between the `bl` and the `add` in the target's bytes, so the
  extra edge is not derivable; 15 more spellings (`ofst + ofst1`, `(Sint32)`/`(Uint32)`/`Sint16`
  temporaries, statement order, volatile read, Sint16 ofst1/ofst2v, ternary n2, `-= -ofst`) all r0.
- mpv_hdec `MPV_DecodePicAtrSj` 21 rows (+4 bytes): both in the skip helpers. EXT: ptr r4 / bitpos r7 in the
  target, ours ptr r7 / bitpos r4 — ours sinks the single-use `bitpos` def into `q`'s computation (a
  temporary coloured before the locals), the target has bitpos as a variable. UD: the target computes
  `ptr` AND `bitpos` before `mpvhdec_AnalyUd` (r26/r29 live across the call) and reloads `ck.data` after
  it; ours sinks bitpos past the call and keeps `data` (r25) instead. Probes p1..p4 (probe files deleted):
  only a conditional branch right after the call (`if (f(..) < 0) return;`, an inlined wrapper with an
  early return) or a second def BEFORE the call anchors the def; `register`, `Uint32`, blocks, do/while(0),
  `for(;;){..;break;}`, `switch (f())`, `r = f()`, `if (f()) {}`, a `q = ck.data` variable redefined after
  the call, `bitpos += 7` after the call, self-assignment, `(Uint8*)ptr + (bitpos>>3)` as the argument
  (+4) all sink. The frontend CSEs the three `mpv->ck.data` reads into @565 first, which is what makes the
  sink legal; the target's IR must have kept a memory operand or a branch there.
- sfd_hds `SFHDS_SetHdr` 11w: target result r30 > len r29 > p r28 > sfd r27 = the parameter order of the
  inlined `sfhds_SetHdrPkt(sfd, p, len, result)`, i.e. its three parameter copies kept as nodes (sfh, the
  nested IsSfdHeader local, above them); ours propagates them (result 28 neighbours, one level, p > len >
  result by id). `void *` parameters with typed locals (propagated: p/len only feed `subi/addi`
  arguments, result a store — no argument move), parameter order, `len` declared before `p` (15w),
  `data -= 6; size += 6` on the parameters (62w) do not keep them.
- cri_cvfs `cvFsGetFileSize` 63w / `cvFsOpen` 240w (-4): the inlined `cvfs_ResolveDev`: target `pdev`
  (own local, r28) coloured before ResolveDev's `tbl` (@1358, r27) although tbl has the higher id — pdev
  needs 29 at its iteration-2 scan (ours 28/55 vs tbl 26/46); target materialises `tbl = cvfs_tbl` after
  GetDevIf's `strlen` and copies `dev = tbl` (`mr r24, r27`); assigning `tbl` just before the GetDevIf
  call gives 45w but cvFsOpen 259w (not applied); GetDevIf declaration orders: no change.
- sfd_cre `sfcre_AnalyMpv` 15w: target colouring order b4 r4, b7 r5, ofs r6, b6 r7, b8..b11 r8..r11, b5 r6
  (b5 last = a two-use variable, b7 second = a variable between b4 and ofs; `ofs + 1` fresh in r0); ours
  has b7 as the temporary @128 coloured first (r5) and ofs first among the variables (r4, in-place
  `addi`). 8 declaration orders / Sint32-Uint32 b7 / picrate_code types: 15-24w.
- sfd_tst `SFTST_Calc` 83w: the target's abs diamond is the if/else ARM shape (`subfic r22; subfze r23,
  r23` in place, else `mr r22, r25`: adiff.hi coalesced with diff.hi) placed BEFORE the inlined
  sftst_Conv; the if/else, `?:` and an inlined `sftst_Abs(v)` helper (return/variable/if-else/ternary
  bodies) all give that arm shape but the frontend sinks the whole diamond below the Conv division call
  into its single use (100w); `adiff = diff; if (diff < 0) adiff = -diff;` (kept, 83w) stays above the
  call but copies both words before the compare; `adiff`-based tests, a `d = (Sint32)diff` before/after:
  83-367w. Same sink class as the mpv_hdec bitpos.
- adx_baif `AIFF_GetInfo` 129w (-8 bytes): the target's header FORM word is a VARIABLE web (3 bytes in
  r30, `mr r27, r30`, byte 3 merged into the copy — the multi-def `ckid` pattern our cksz already shows)
  while ours propagates the single-use header `ckid` into the compare (one r10 temp), and the target's
  swapped size is a temp (`rlwinm r12..` feeding `end`) where ours keeps it in the variable (r27);
  `end = p + (SWAP32(cksz) - 4)` forms, end-first, a dead second use: 137-138w.
- objects.py / AGENTS.md were modified by other agents every few minutes this pass; edits were surgical
  single-anchor inserts.

### CRI pass 13b: the frontend's IF and loop shapes read off the branch pairs (mps_lib MPS_Init 48 -> 12w, sfd_buf RingAddWrite/AddRead 68/136 -> 16/22w; no function flipped; pure C; 2026-09-11)
Harness ~/.cache/cri13b/ (deleted): cri13's `bld.sh`/`bytecmp.py`/`tryvar.py`/`fd.py` copies
plus a sparse clone of mk-deception's sfd_tim/mps_lib/sfd_buf/adx_sje/mpv_umc/sfd_mps/adx_bsc/mpv_mcy/
cft* sources (decompiled style, written-out bodies: no structure information, confirmed again). ~110
variants, every one predicted from a dump first; the two gains and the four negatives below are all
frontend facts, not allocator facts.

**Read off the dumps (verified):**
- **The frontend unrolls counted loops itself** (`frontend-01`: the 8x body, `@216 = num - 8`, the
  `num > 8` guard, `GOTO @211` into the remainder test) and emits the loop's zero-trip guard as
  `IFNGOTO @exit (0 < num)`; `do { } while (i < num)` (any spelling, `++i` in the test, an explicit
  `if (num <= 0) return` guard) and every `for (;;)`/`while (1)`/goto loop with a `break`/`return` exit
  are NOT unrolled (0xd4-0xe4 bytes instead of 0x144). The unroller works on `for`/`while` only.
- **The inlined helper's argument expression outranks the unroller's temporaries**: mps_lib
  `mpslib_ClrHn(MPSLIB_libwork->hn, num_hn)` makes the handle base an argument temporary (`addi r4,
  r3, 0x10` = coloured before the `num - 8`/pointer copies, which get r6/r7) where a caller local `hn`
  is coloured last (r7) — with the same `li r5, 0` shared by the three clears and the helper's `i`
  (pass 12b/10b rule). 48 -> 12w; `lw`-parameter helpers (`hn = lw->hn` inside) are identical.
- **A parameter copy made for an inlined helper that MODIFIES its parameter is created after all the
  helper's locals** (@132 above @116..@127 = lowest id, r12): so no helper-parameter form can place a
  value between two helper locals (sfd_tim `tunit` below).
- **The frontend's IF lowering is `IFNGOTO @else cond; then; GOTO @join; @else: else`; with an EMPTY
  then-arm (also `;`, `(void)0`, `(void)x`, a dead store, an empty inline call, `x = x`) it inverts to
  `IFNGOTO @join !cond`** — the target's `bne body; b end` for sfd_buf's `nbyte == 0` exit is therefore an
  `if (nbyte == 0) { A } else if (..) { ret = 0; } else { body }` chain whose arm A generated NO code in
  the original but was not empty at lowering time. `ret = 0` in A gives the chain with one extra `li`
  (68 -> 16w, 136 -> 22w, same sizes); `do {..break..} while (0)`, `for (;;) {..break;}`, `while (1)`,
  goto forms and `if (c) break; else if` are all dissolved into the plain `beq end`.
- **The backend's add-propagation folds `addi ring, hn, K` into every load/store AND into a later
  `addi tot, ring, 0x20` (addi-into-addi), across blocks and calls**, extending hn's live range into a
  callee-saved register; casts (`(SFBUF_RING *)&hn->w.u`, `(void *)`, `(Uint8 *)hn + 0x1318`), a
  `void *` AddTot parameter with a typed local, `volatile` totals, `volatile SFBUF_RING *`, ring-typed
  AddTot helpers and expression-only forms (`&SFBUF_GET_HN(sfd, n)->w.u.ring`) never stop it. The
  target's `ring` node (`addi r28, r3, 0x1318`, hn dying in r3) needs a use of `ring` that is not a
  load/store/addi at add-propagation time (a `mr`/compare/call argument that a LATER pass removes) — not
  found in C.
- Frontend constant folding runs after inlining and deletes dead arms completely: `if (flag)` with a
  constant helper argument, `if (1)`, `sizeof` tests, `switch (0)`, `if (num == num)` all leave no
  block; a runtime `if (num > 0) { for-loop } else { return; }` in a void helper that also contains the
  two Init calls reproduces MPS_Init's `b .Lcalls; b .Lreturn` pair and the target's `li r3, 0` return
  block exactly, but keeps its own `cmpwi; ble` next to the loop guard (40w). The target's dead pair =
  an if/else whose condition vanished WITHOUT the frontend deleting the else, i.e. a test the backend
  folded (a two-definition variable compared right after its constant store is NOT folded: 49w).

**Applied:** mps_lib `MPS_Init` (static `mpslib_ClrHn(MPS hn, Sint32 num)` helper, called with
`MPSLIB_libwork->hn`; 48 -> 12w, the pair + `li r3, 0` block remain), sfd_buf `SFBUF_RingAddWrite/
AddRead` (the if/else chain; 68 -> 16w, 136 -> 22w: the extra `li ret, 0` and `ring` as a node remain).

**Residues (exact class):**
- sfd_tim `SFTIM_IsGetFrmTime` 6w: target colouring order ftime, vrate, tunit, tscale, ncount = tunit
  created between the helper locals vrate (@122) and tscale (@124), i.e. where `adj` is declared. 30
  forms: helper-local copies of the parameter (plain, `Uint32`/`Sint16`/pointer/cast sources, two-def,
  block-scoped, killed-after `unit = 0`) are all frontend-propagated or dead-store-eliminated, `tunit + 0`
  /`(Sint32)(Uint32)tunit` arguments are folded, a modified parameter's copy is created LAST (@132), the
  written-out body (mk-deception's form) reorders the loads (45-60w). `SFTIM_IsStagnant` 2w: the else
  arm's left-operand-first load order with the result in r0 needs an r0-blocked `chg_base` temporary;
  `d = ext_cnt; unit = ext_unit; d -= chg_base` gives the order with d in r3 (5w), `volatile` on the
  left operand, `-(b - a)`, `a + -b`, statement swaps: 2w.
- mps_lib `MPS_Init` 12w (above), `MPS_Create` 2w (post-RA `li r4, -1` / `addi r0` tie: 11 statement
  orders of the -1 stores / `x10 = 2` / `dechd_func` move the pair or cost 4-64w).
- sfd_buf `SFBUF_RingAddWrite/AddRead` 16w/22w (above); `SetSupplySj`/`DestroySj`/`InitHn` untouched.
- Not reached this pass: adx_sje/mpv_umc 1-2w ties, sfd_mps, adx_bsc (.data 8 vs 0 + `skg_version`),
  mpv_mcy, cftyp422_ppc (.bss order), cftfx (deferred-inline order).
- Build hazard again: `ninja -k 0` from several agents at once loops on the objdiff.json race for 20+
  minutes (nj.sh retries); compile the unit with `ninja -t commands` (bld.sh) and run the full check once.

### CRI pass 14: nested-assignment anchors and word-pointer steps (mpv_hdec Matching; sfd_tst 83 -> 79w; 2026-09-11)
Harness ~/.cache/cri14/ (deleted): cri13b's `bytecmp.py`/`tryvar.py`/`fd.py`/`bld.sh`/`mwcc.sh`
(probe compile with a unit's flags + `dtk elf disasm`); tools/research/mwccdbg reused. Every finding below was read
off one dump (`frontend-01` for what the frontend kept, `backend-00` for the codegen tree, `backend-1N` for
the post-RA peephole) before the build.

**Model additions (verified with probes):**
- **A nested assignment blocks the frontend's forward substitution.** `x = expr` with a single use is
  substituted into the use (past calls too: the def's operands are locals, so nothing stops it — the
  pass-13 "single-use def sunk past a call" class). When the def's expression CONTAINS an assignment
  (`bitpos = ((Uint32)d - (Uint32)(ptr = (Uint32 *)((Uint32)d & ~3))) << 3`), the side effect makes it
  non-substitutable: `bitpos` stays a variable, coloured by its declaration position (mpv_hdec: ptr r4 /
  bitpos r7 in the EXT skip; ptr r26 / bitpos r29 live across the AnalyUd call in the UD skip). The same
  anchor keeps the 64-bit abs diamond above a call: `adiff = ((diff = a - b) < 0) ? -diff : diff;` (sfd_tst
  `SFTST_Calc`: the ECONDASS of `adiff` is otherwise substituted into `excess < adiff` below the
  sftst_Conv call; if/else and `?:` forms, self-assignments, `adiff = (adiff = ..)` nests, dead `diff = 0`
  defs all sink). Nesting a plain COPY (`p = (p = buf + i) + 4`) does not anchor: the copy is propagated
  first.
- **Codegen folds a constant into the non-pointer operand of an add chain**: `(ptr + n) + 4`, `ptr + n - 8`,
  `(Sint32)ptr + n + 4`, `(Uint8 *)((Uint32)q + 4)`, `&q[4]` all emit `addi n', n, K; add ptr, n'` (the sum
  after the constant), whatever casts sit between. Two ways to get the target's `add q, ptr, n; addi/subi
  r0, q, K`: (a) q a VARIABLE stepped by `q += n` (MPVBIT_BYTEPTR's shape) — then q is a frontend @temp
  ranked BELOW the backend temporaries (the `ck.data` reload takes r4, q r6); (b) **the constant as a
  word-pointer step, `(Uint8 *)(ptr + 1) + n` / `(Uint8 *)(ptr - 2) + n`**: codegen pulls the scaled
  constant out to the end, the sum is a backend temporary created after the reload (right operand first),
  so it is coloured before it and takes the argument register (target `lwz r7, ck.data; add r4, ptr, n;
  addi r0, r4, 4; subf r4, r7, r0`). (b) is byte-identical at all five mpv_hdec sites and REPLACED the
  pass-7 asm `lwz data` pins (MPVHDEC_FLUSH, mpvhdec_DecSlice loop test and tail) — the `register` on
  DecSlice's parameter went with them.
- **The stwbrx fold is the post-RA peephole** (backend "after-peephole" following prologue/epilogue): it
  matches the merged `rlwinm 8,8,15; rlwimi 24,0,7; rlwimi 24,16,23; rlwimi 8,24,31; stw` on the SAME
  source register. Every linear-OR spelling of the swap (term order, casts, Sint32/Uint32, volatile store,
  a `static` swap helper, a `static` store helper, `| 0`, a second use of the swapped local) is merged
  pre-RA and folded; only pairwise associations `(a|b)|(c|d)` or `s |= ..` statements keep an `or` (and
  then are not the target's chain either). sfh_main's target has the same five instructions, same
  registers except the word (r6), unfolded: not a source shape in this compiler's post-RA peephole — the
  M4 `#pragma peephole off` + asm chain stays.
- **The frontend CSEs `(Uint32)(mpv->ck.data)` across two macro uses into one @temp but does NOT CSE a
  struct-field read across a call** (the `q - mpv->ck.data` after `mpvhdec_AnalyUd` is a fresh
  EINDIRECT in both builds); an `extern` callee makes no difference.

**Fixed:** mpv_hdec `MPV_DecodePicAtrSj` 21 rows -> 0 (Matching, 15/15): MPVHDEC_SETPOS_KEEP (the nested
`ptr =`) + MPVHDEC_SKIPWORD (`(Uint8 *)(ptr + 1) + ((bitpos + 7) >> 3)`) in the two skip helpers; the FLUSH
pins removed as above. sfd_tst `SFTST_Calc` 83 -> 79w (not flipped, see below).

**Residues (exact class, forms tried):**
- sfd_tst `SFTST_Calc` 79w: with the anchor the diamond has the target's shape (`beq; subfic r22; subfze
  r23, r23; b; mr r22, r25`) but diff's pair are lo r23 / hi r25 (target lo r25 / hi r23): the backend
  copy-propagates `mr diff, sub` so diff is the backend pair r226/r227 (lo lower vid), adiff.hi (@119)
  coalesces INTO diff.hi's copy (the lower vid survives) and diff.lo (vid 226) is coloured before the
  merged hi (vid 96). The target needs diff kept as a variable (own-local vids below adiff's @temp) — a
  live second definition of `diff` (`diff = sftst_Conv(..)` right after gives the target's pair, wrong
  semantics); `diff` reused for t/step/adj/aave (disjoint webs, range-split away), dead `diff = 0` inits,
  ECOND inside the compare with the call, 6 declaration orders: 79w.
- sfd_hds `SFHDS_SetHdr` 11w: the target's `result` node has a vid between the inlined IsSfdHeader's `sfh`
  (@231) and the own locals (len > p, i.e. `Sint32 len; Uint8 *p;`): a frontend @temp created AFTER the
  nested inline, holding result. Every `res = (Sint32 *)(void *)result` copy (5 placements, a `void *`
  local, helper `void *` parameters for result/p/sfd) is kept by the frontend and removed by the backend
  copy propagation (no argument move exists for result: pass-12 rule confirmed on the dumps); a kept sfd
  copy `s = (SFD)(SFD_OBJ *)sfd` (argument move after calls) pushes result to 29 neighbours = level 2 =
  r31 above sfh (4w, wrong order); parameter reuse `data -= 6; size += 6` (62w), the two-site return
  form, a body helper with p/len as parameters (44w).
- adx_tsvr `adxt_nlp_trap_entry` 2w: unchanged (the `lha r4` load feeds only the `add`; no r0-coloured
  value and no argument use in the target's bytes) — not touched this pass.
- adx_baif `AIFF_GetInfo` 129w: the header FORM word is substituted into the compare (single use) while
  the target keeps it as a variable (3 bytes in r30, `mr r27, r30`, byte 3 merged into the copy = the
  shape our 2-def `cksz` has); the target's swapped size is a single-use temp (`rlwinm r12, r28 ..; subi
  r10, r12, 4; add r10, r8, r10`). `ckid` reused for the AIFF word, `if ((ckid = LE32(buf)) != FORM)`
  nests, `end = p + (SWAP32(cksz) - 4)` swap-as-temp, a stepping-p header, `||`-joined checks: 138-150w.
- sfd_cre `sfcre_AnalyMpv` 15w: b7 is CSE'd into an int @temp (both uses convert it), coloured first
  (r5); the real difference is the `ofs + 1` temporary: coloured before the byte variables in ours (takes
  r4 in place of the dying ofs) and after b4/b7 in the target (r0, `addi r0, r6, 1`) although it is a
  backend temporary in both. `Sint32/int/Uint32 b7` between b4 and ofs in 8 orders: 15-26w.
- cri_cvfs `cvFsGetFileSize` 63w / `cvFsOpen` 240w: `tbl` materialised after GetDevIf's strlen in the
  target (`addi r27, r31, 0x144; mr r24, r27`); no `tbl` local / `dev = tbl = cvfs_tbl` inside GetDevIf
  (114w/267w), `tbl = cvfs_tbl` before the GetDevIf call (45w/259w, pass 13) — not applied.
- mpv_hdec `mpvhdec_DecSeqUdsc` keeps its pass-7 asm `add p, buf, i; addi p, p, 4` pin: `(buf + 4) + i`,
  `&buf[4] + i`, `(Char8 *)((Uint32)buf + i) + 4` fold to `buf + (i + 4)`; `(Char8 *)((Uint32 *)buf + 1)
  + i` hoists `buf + 4` as a loop invariant (87w); `p = buf + i; p = (Char8 *)((Uint32 *)p + 1)` gives
  the order but the sum in a temp (`add r3; addi r26, r3, 4`, 2w) — the target's `add p` is in place,
  i.e. p a variable with the +4 as its second definition.

### CRI pass 14b: backend-folded tests, add-propagation's block rule, counters above IV temps (sfd_tim 37 -> 38/39, mps_lib 5 -> 6/7, adx_sje 11 -> 12/17, mpv_umc Forward/Backward 37 -> 2w, sfd_mps 21 -> 22/26; no unit flipped; pure C; 2026-09-11)
Harness ~/.cache/cri14b/ (deleted): cri14's `bld.sh`/`bytecmp.py`/`fd.py`/`tryvar.py` copies;
tools/research/mwccdbg reused. Every fix was predicted from one dump (or one pass-13b sentence) and confirmed
in 1-3 builds; the negatives below are stated with the mechanism the dump showed.

**Read off the dumps (verified):**
- **A test the backend folds but the frontend keeps = a status variable assigned from an inlined helper's
  constant return.** `ret = helper(..); if (ret != 0) return ret;` with `static Sint32 helper() { ..; return 0; }`
  keeps the IF at lowering (`IFNGOTO @join (ret != 0); mr r3, ret; b epilogue; @join:`), the backend then
  proves `ret == 0`, turns the IFNGOTO into `b @join` and cross-jumps the dead `li r3, 0; b epilogue`
  with the return block's `li r3, 0` — mps_lib `MPS_Init`'s `b .Lcalls; b .Lreturn` pair and its `li r3,
  0` at the head of the return block, 12 -> 0w. `if (helper() != 0) return 0/-1;`, `goto end`, `if
  (helper() == 0) { calls }` all lose it (the frontend folds the call result compared directly).
- **An inlined helper's `ret = 0` in the FIRST arm of an if/else chain vanishes** (sfd_buf's `bne body; b
  end` shape, pass 13b): the helper local is a @temp, so the backend CSE rewrites its `li 0` into a copy of
  the entry `ret = 0` (an extended-basic-block CSE: the first arm has a single predecessor, the later `else
  if` arm is a join block and keeps its `li`) and coalescing deletes the self-copy. An own local's `li` is
  never rewritten (pass 12b rule). Applied structure for sfd_buf not kept (57w: `ring` below).
- **Add-propagation folds `addi rD, rA, K` into rD's uses only when rA is defined in the SAME basic block as
  the addi** (pass 13b's "across blocks and calls" is about the USES, which may be anywhere). Seen on
  sfd_buf: an `addi tot, ring, 0x20` created in a join block after the calls stays as a node (the peephole
  folds it post-RA); the same addi in the entry block folds. Block boundaries in the IR: labels (every
  if/else join, inlined-helper return labels), the parameter block B1 (`mr r32, r3`... — a parameter's
  register is defined in B1, so an addi off a parameter in B2 is NOT folded), and every call ends a block.
  The frontend embeds a nested-inline argument temp's assignment inside its first use (`ring = (@215 =
  sfd + n*0x74) + 0x1318`), so the argument and the addi share a block. sfd_buf's target `ring` node (addi
  in the entry block, hn's `add` in the same block by the final bytes) therefore had an IR boundary
  between hn's definition and ring's addi that left no code — not found (T1..T9: helper parameters,
  `void *` typed copies, two-level helpers, argument expressions: all folded; a `mr` base defined by a kept
  copy is also folded).
- **A one-use `wk = &mpv->x` is propagated into its call argument by the frontend; with TWO uses (a field
  load `wk->work` and the pointer itself as a later call argument) it stays a variable, the field load is
  folded by add-propagation and the argument move `mr r3, wk` cannot be, so `wk` lives across the first
  call in a callee-saved register** (mpv_umc `MPVUMC_Forward/Backward` 37 -> 2w each, +0xc bytes of
  prologue: `mpvumc_OneReadMb(mpv, wk->work, ..)` instead of `mpv->mcwk.work`).
- **Loop counters above the strength-reduction pointers, in a public server callback**: adx_sje
  `ADXSJE_ExecHndl(void *obj)` + `ADXSJE sje = obj` (kept copy, sje r31 first) with the header stage as a
  `static inline` helper (own locals rank below the IV temps, helper locals above them, pass 13) and the
  two stepping pointers written as `Uint8 *p/p2/p3 = (Uint8 *)sje` locals stepped by 4/2/4 with `*(SJ *)(p
  + 0x4)` / `*(Sint16 *)(p2 + 0x2c8)` loads (adx_tsvr's pass-11 idiom: `mr rIV, sje` + offset). Colouring
  = reverse declaration order with "lowest free handed-out callee-saved register, else a new one": `n, c1,
  c2, ck, prd, p3, p2, i, p, sjo, ch` gives ch r30, sjo r29, p r28, i r28 (coloured before p2 so it reuses
  r28), p2 r27, p3 r27, prd r29; the chained store is `first[ch] = first2[ch] = v` (inner store first).
  35 -> 0w. A plain `static` helper of that size is not auto-inlined (`static inline` needed).
- **A helper local is created before the return temporary of a helper inlined inside it**: sfd_mps
  `sfmps_ProcPrep`'s see-header tail as `static inline sfmps_ProcPrepSee(sfd)` with `w` a helper local:
  `w` (created at inlining) outranks the nested `sfmps_GetSeeShdr`'s @ret (created when that call is
  inlined), so `w` takes r4 and the shdr temp r5 (target), where an own `w` ranks below the temp and shdr
  takes the dying r4 in place. 30 -> 0w.
- **`d = a; base = *(volatile *)&b; d -= base;` with `base` declared before `d`** loads the left operand
  first and keeps the result in r0 (sfd_tim `SFTIM_IsStagnant` 2 -> 0w): the two-def `d` is coloured before
  `base` (later declared = higher id) and takes r0, `base` r3; the expression `a - b` loads the right
  operand first (right-operand-first temp creation), `d = a; d -= b` (no volatile) gives d in r3 because
  the chg_base temp is coloured first. The unit already uses the volatile re-read idiom (`tim->vcnt`).
- Post-RA ties, reconfirmed on the dumps: mps_lib `MPS_Create` (pre-RA order = target's `li r4, -1;
  addi r0`, the post-RA list scheduler swaps them; all registers identical to the target's, so the DAG edge
  the target has is not visible in the bytes) and mpv_umc `Forward/Backward/BiDirect` (`mr r3, wk` hoisted
  one slot above `lwz r0, ofs` — here the PRE-RA scheduler already picks `mr r3, wk` before `addi r4,
  &ccnt_rt`, the target the addi first; the arguments are emitted left to right in ours). Left as 2w.

**Applied:** sfd_tim `SFTIM_IsStagnant` (0w), mps_lib `MPS_Init` (0w), adx_sje `ADXSJE_ExecHndl` (0w),
mpv_umc `MPVUMC_Forward`/`MPVUMC_Backward` (2w each), sfd_mps `sfmps_ProcPrep` (0w). No flag flipped.

**Residues (exact class):**
- sfd_tim `SFTIM_IsGetFrmTime` 6w: `tunit` must be created between the helper locals vrate (@122) and
  tscale (@124) although it is loaded BEFORE the `cond[14]` test (i.e. it is an argument temporary of the
  Tunit call, @112, created before all helper locals); inlined-helper locals are numbered in reverse
  declaration order at inlining, so no single-helper declaration order can place an argument temp there,
  and a typed/`void *`/two-def/cast copy of it inside the helper is always frontend-propagated (dump: the
  copy's @ is 0/0). 9 more forms.
- mps_lib `MPS_Create` 2w (post-RA tie, above). `.bss`: the split object carries an extra `lbl_80309B54`
  label (also sfd_tim `lbl_8030835C`, sfd_mps `lbl_802F629C`, adx_sje `lbl_8031027C`): bytecmp's order
  check lists them; sizes and bytes are identical.
- sfd_buf `SFBUF_RingAddWrite/AddRead` 16w/22w: the two mechanisms are identified (helper-local `ret`
  for the arm, an IR block boundary before `ring`'s addi) but the boundary has no C form yet; T1 (body as
  `sfbuf_RingAddWriteHn(sfd, hn, nbyte)`) gives the target's `bne; b` and sfd r30 / nbyte r31 but folds
  `ring` (57w) — not applied.
- adx_sje `adxsje_encode_data` 84w / `set_rsig` 85w / `calc_rsig` 178w: register permutations (sje r29
  below sjo/ret in encode_data = another kept-copy/helper-level question), untouched; `output_header` /
  `write_end_code` 2w ties.
- mpv_umc `MPVUMC_Intra` 35w (volatile-register permutation of the six block-pointer stores, `mr r5, r3`
  copy of mpv in the target), `mpvumc_OneReadMb` 91w untouched. `mpvumc_OutputIntra6blk` is still a
  whole-function `asm` block in the source (pass-8 rule violation, predates this pass; not mine to remove
  without its C shape).
- sfd_mps `sfmps_ExecServerSub` 59w: the loop + counters as an inlined helper gives the target's `li r26,
  0; mr r30, r26; mr r28, r26` (helper-local temps share the zero, pass 12) but sfd stays r25 (target r31 =
  level 2): 51w, not applied. `CopyPrvate` 60w / `CopyPketData` 157w / `DecodeOneUnit` 412w untouched.
- Not reached: adx_bsc (.data 8 vs 0 + `skg_version`), mpv_mcy, cftyp422_ppc, cftfx.
- Build: two `flock ... ninja -k 0` runs showed transient `FileNotFoundError` on SDK objects (wibo compiles
  producing no .o under load from other agents' builds); the third run and the sha check passed with
  nothing flipped.

### CRI pass 15: dead compares, helper-local arms, add-propagation's real blocker (sfd_mpv 14 -> 15/38; no unit flipped; pure C; 2026-09-11)
Harness ~/.cache/cri15/ (deleted): `bld.sh lib/unit` (exact ninja command + bytecmp), `tryvar.py
lib/unit variants.py [Func..] [--fdiff Sym] [--keep LABEL]` (text-replacement variants compiled into a scratch
dir and copied over the unit's object for bytecmp; `fd.py` = tools/fdiff.py without its `ninja` step, which
otherwise rebuilds the real object over the variant), tools/research/mwccdbg reused. Every claim below was read off
a dump or a variant build.

**Read off the dumps / variants (verified):**
- **A compare without a branch = an IF whose branch targets the next instruction.** The frontend keeps
  `IFGOTO L; <L>: RETURN` when the arm is a `return` at the END of a void function (`if (c) return;` last in
  the body), and the backend deletes the jump-to-next but never a `cmp`. In a value-returning function every
  spelling with equal return values (`if (c) return 0; return 0;`, `if (c) ret = 0; else ret = 0;`, `?: 0 : 0`)
  becomes an ECOND lowered branchless (`cntlzw/extrwi/neg/andc`), a bare `return;` gives `beqlr; li r3, 0`,
  goto-to-next-label / empty arms / `(void)x` / dead locals / an empty inlined call are all deleted by the
  frontend. **The C form is the pass-14b helper-local rule**: `static Sint32 sub(SFD sfd) { Sint32 ret = 0;
  if (SFMPV_WK(sfd) == NULL) { ret = 0; } return ret; }` + `SFMPV_Stop = return sub(sfd)`: the arm's `ret = 0`
  is a @temp `li`, the backend CSE rewrites it into a copy of the entry zero, coalescing empties the arm and
  the branch to the join is dropped — sfd_mpv `SFMPV_Stop` = target `lwz; li r3, 0; cmplwi r0, 0; blr` (0w,
  15/38; as an OWN local the arm's `li` stays and gives the 1w `bnelr` form; pass 9/10 forms explained).
- **Add-propagation's block rule (14b) is not the mechanism**: mpv_cmc `MPVCMC_InitMcOiRt`'s `addi oi, r32,
  0x124` sits in B2 with `mr r32, r3` in B1 and its stores in B4 (after the ccnt diamond) — folded anyway.
  What blocks the fold is a non-load/store use of `oi`: `if (oi == NULL) return;` gives the target's registers
  with the `cmplwi; beq` left (2w, +8 bytes); `if ((void *)oi == (void *)mpv) return;` (last statement) leaves
  only the dead `cmplw r5, r3` (1w, +4). A multi-def `oi` (`oi = mpv->oi_rt` in both arms: 2 addi, 14w;
  do-while stepping: real loop), `volatile`, a `static` getter, `oi` after the if, `pc + 1` derivation, dead
  `oi = NULL`, `#pragma opt_propagation/opt_common_subs/opt_lifetimes/opt_loop_invariants/opt_strength_red
  off`, `optimization_level 3` do nothing (`peephole off` turns the ECOND into if/else arms: 12w). The
  target's `work` pointer in InitObj (`addi r4, r31, 0xd00` before the cmpwi) is the same class (an address
  value defined before the diamond and not folded). Same open class as sfd_buf's `ring`.
- **The stwbrx fold does not depend on the word's register**: an asm `lwz r6` pin of the word + the merged
  chain under peephole ON still folds (`stwbrx r6, r0, r4`). **The post-RA peephole does NOT merge
  `rlwinm t, w; or s, s, t` pairs** even when adjacent with `w` intact and `t` dead (asm-spelled unmerged
  chain, peephole on: the first `or` merged pre-RA via `mr + rlwimi`, the other two stayed `or`). So the
  target's merged chain was merged PRE-RA like ours (peephole-forward: `or r47, r45, r46` -> `mr r47, r46;
  rlwimi r47, w, ..` with the dead `rlwinm r43..r45` kept until RA and NOT in the interference graph), and the
  word's r6 (readers) / r6 with r4 skipped (SmpHz) means ONE more node coloured r5 / r4 in the target's graph
  — hdr / id dead in ours at the load, a dead partial-result temp would fit both. Pairwise `(a|b)|(c|d)` and
  `s = c|d; *val = a|b|s` give the word r6 with one `or` left (4w); `s |= term` statements (any order),
  `s = s | term`, `+`/`^` chains: the pre-RA scheduler hoists the rlwinm's above the ors and the LAST term is
  computed in place into the word's register, so two `or`s remain (8w). M4 stays as it is (asm chain +
  `#pragma peephole off`, 1w x6).
- sfd_hds `SFHDS_SetHdr` (11w): the target order result r30 > len r29 > p r28 > sfd r27 IS the parameter
  order result(r36) > size(r35) > data(r34) > sfd(r32) — `data -= 6; size += 6; ... data -= 2; size += 2`
  gives exactly that order but every node gains 2 neighbours (29-30: the second start code's bytes 2/3 are
  no longer CSE'd with the first's p[0]/p[1] because the loads fold through r5/r34 instead of one `p`),
  so all four jump to level 2 above sfh (result r31 .. sfd r28, 62w). A modified-parameter copy in the inlined
  SetHdrPkt (`*result++ = ..`) is created at @227, BEFORE the nested IsSfdHeader's @228..@231 (13b's "after
  the helper's locals" = before nested inlines), and the dead `addi` is deleted by add-propagation after
  copy-propagation has already folded the copy. `res = result; result = NULL;` (frontend deletes the dead
  redefinition and propagates the copy), a ChkPkt helper returning `result`/NULL (a 2-def @ret, compares
  the pointer): not the target.
- sfd_cre `sfcre_AnalyMpv` (15w): the whole residue is ONE interference: pre-RA ours schedules `rlwinm
  r50 (b7>>4&0xF); addi r49 (ofs+1); cmpi r50` so r49 and r50 interfere (r50 -> r0, r49 -> r4 in place of
  ofs, b4 -> r6); the target has `extrwi.` before `addi r0, r6, 1` (no interference: both r0, b4 r4, b5 the
  dying ofs's r6). Statement order (size update first/last, before the loads, `size = size - ofs - 1` is
  reassociated to `ofs + 1`), `size -= ofs; size--` (25w), `1 + ofs`, `(p + 1) - data`, picrate_code before
  the test, `(b7 & 0xF0)`, 20 declaration orders x 4 b7 types (b5 last gives b5 r11: the register order of the
  bytes follows from ofs+1's colour alone) all keep the addi before the cmpi. The scheduler's tie-break here
  is not source order (the loads come first in the IR and are scheduled after the addi).
- sfd_tst `SFTST_Calc` (79w): comma forms of the anchor (`(diff = ..), (diff < 0) ? ..`, comma inside the
  condition) are split into statements by the frontend and sink again (100w); `if ((diff = ..) < 0)` arms
  117w; `-(diff = diff)` 100w. Not moved.
- adx_tsvr `adxt_nlp_trap_entry` 2w: not touched (passes 11-14 exhausted the r0/r4 question).

**Applied:** sfd_mpv `SFMPV_Stop` -> `sfmpv_StopSub` helper (0w). Nothing flipped; sfd_hds, sfd_cre, mpv_cmc,
sfh_main, sfd_tst sources unchanged. Not reached: sfx_zmv, adx_baif, sfx_cnv, cri_cvfs, dct_ac, sfd_adxt,
mwsfdcre, the other sfd_mpv functions.

### CRI paired-single kernels pass 1: mpv_mc / mpv_mcy motion-compensation kernels to C (mpv_mcy asm placeholder -> pure C, 85 -> 689w with 3/4 function sizes exact; mpv_mc kept Matching with its asm; 2026-09-11)
Harness ~/.cache/cri_ps1/ (deleted): `try.py unit file.c [Func..]` (unit flags compile + strip_unused +
bytecmp with OBJ override + side-by-side objdump), `sched.py unit Func file.c [models..]` (register-BLIND
similarity of a function's instruction stream: mnemonics + immediates with every rN replaced — the only metric
that ranks schedule variants, word counts are noise once registers shift), `region.py unit Func file.c --from RE
--to RE [-n K] -v` (the same on one loop body), `cc.sh probe.c` (CRI flags + objdump), `gen4.py`/`genh2*.py`
(source-variant generators), tools/research/mwccdbg reused for raw/scheduled PCode and the priority list.
**Premise correction:** the eight `asm` kernels of mpv_mc.c/mpv_mcy.c are INTEGER SWAR code (byte averages through
0x01010101/0xFEFEFEFE masks, rlwimi packing), not paired-single; nothing in them needs `__PS_*`/GQRs. dct_fsri's
kernel (pass 9) is an `asm` block with `register __vec2x32float__` operands, not intrinsics. github.com/
ShulkMaster/mk-deception has C for both units (`sfdcore/mpv/mpv_mc.c`, `mpv_mcy.c`): its semantics are right
except MPVMC16 H2 case 0's last byte (`lbz s[16]`, not `words[4] >> 24`) and the 1p pitches (`(Uint32)stride &
~7/~3/~1` = `clrrwi`, not signed `/ 8`), but its shapes reload after every store and are 1200+ words off.

**Shapes that reproduce the target's arithmetic and loop structure (verified by region diffs):**
- Four-point sum: `s0[0] + s0[1] + s1[0] + s1[1] + 2` — the frontend pulls the FIRST leaf out and adds it last
  (`add a1+b0; add +b1; addi 2; add a0+`), exactly the target; `a0 + (a1 + b0 + b1 + 2)` (mk-deception) is
  wrong. Pack: `((p0 << 22) & 0xFF000000) | ((p1 << 14) & 0x00FF0000) | ((p2 << 6) & 0xFF00) | ((p3 >> 2) &
  0xFF)` gives the target's `rlwinm p1; rlwimi p0; rlwimi p2; rlwimi p3` (the OR chain `((A|B)|C)|D` is
  evaluated D, C, A, B raw and merged pre-RA); `(Uint8)(p >> 2) << n` spells the same code.
- Byte average, 16x16 units (mpv_mcy): `(w & a) + ((x & 0xFEFEFEFE) >> 1) + (x & 0x01010101)` with `x = w ^ a`
  a variable (3 uses) — the reassociated tree `wa + (sh + m2)` IS the target's (`add sh+m2; add wa+t`); the
  masks are per-loop constants (`lis/addi` in each case's preheader) as in the target.
- Byte average, 8x8 units (mpv_mc "TuneC"): the target adds `(w&a) + (x & m2)` first then `+ ((x & m1) >> 1)`
  with operand order (t, sh) — NO 3-term spelling gives it (every permutation/parenthesisation, signed or
  unsigned, is reassociated with the most complex term pulled out as the outer LEFT operand); it needs a
  two-definition temporary `t0 = w0 & a0; t0 += x0 & m2; d[0] = t0 + ((x0 & m1) >> 1);` (single-def `t0 = A + B`
  is propagated and reassociated; a variable with a second definition is not, and statement order is then
  irrelevant — the frontend sinks each definition chain to its use). Its masks are hoisted ABOVE the switch
  (`lis r5; lis r4; ...; subi r11; addi r12` in the entry block, r11/r12 = LOW priority = variables): a
  constant-initialised local is propagated into every loop; only a second definition (`if (stride == 0) m1
  = m2 = 0;`) or `#pragma opt_propagation off` / `opt_dead_assignments off` / `global_optimizer off`
  materialises it once at the top with the target's registers — the real spelling is still unknown.
- `(w0 << 8) | (w1 >> 24)` with w0, w1 live afterwards = `srwi t; mr t2, t; rlwimi t2, w0` (the `mr` is the
  pre-RA or->rlwimi merge whose coalescing fails at high degree; it appears in the 16x16 targets and in ours
  only where the local degree is as high — a schedule-dependent residue). `A | B` evaluates B first: the
  target's `slwi w0<<8; rlwimi w1` (8x8 H2 case 0, no mr) is `__rlwimi(w0 << 8, w1, 8, 24, 31)` or `(w1 >>
  24) | (w0 << 8)`; `(u0 << 16) | (u1 >> 16)` as `rlwimi u0, u1, 0, 0, 15; rotlwi 16` (8x8 H2 case 1) is ONLY
  `__rlwinm(__rlwimi(u0, u1, 0, 0, 15), 16, 0, 31)` — this 2.4.7 has no rotate idiom, so the 8x8 TuneC source
  used the `__rlwimi`/`__rlwinm` intrinsics at least there (and `w1 = __rlwimi(u1 << 8, a1, 24, 24, 31)`
  reads byte 8 out of the already-merged `a1`).
- `__dcbt(p, stride)` = `dcbt p, stride` (`dcbtct` in objdump), `__dcbt(p, 0)` = `dcbt r0, p`. A dcbt is a
  memory barrier for the scheduler: loads before it in the raw order stay before it — the 8x8 4p target
  (`lbz a0; lbz b0; dcbt; ...`) loaded `a0 = s0[0]; b0 = s1[0];` BEFORE the `__dcbt`, everything else after.
- Every byte/word is loaded once in the targets: values must be in variables before the first store (a
  `Uint32 *` store makes the frontend reload every `Uint8`/`Uint32` read after it — no type-based aliasing).
  16x16 4p: two halves (bytes 0-8 -> d[0], d[1] stored, THEN bytes 9-16 with a8/b8 carried in variables).
- Loop shapes: `for (i = 0; i < 16; i++) { ...; d += 2; if (i == 7) d += 16; }` = the target's `cmpwi i, 7;
  bne; addi d, 0x40` with `i` kept next to CTR. 8-iteration loops with a small body are unrolled by the
  compiler: x2 for the H2/V2 rows (`li 4; mtctr` + two identical copies, the second copy's variables become
  `@N` range splits), fully for the 1p copy rows (`for (8) {4 loads; 4 stores}` twice around `d += 16` = the
  target's straight-line 16 rows with the `@N` copies rotating through 4 registers). A 7-iteration loop with an
  `if` inside is NOT unrolled; the 16x16 1p case 0 (doubles) is hand software-pipelined in the source: `a = row;
  b = next; store a; a = next; store b; ...` written out (16 rows, 4 FPRs), reproduced exactly.
- 1p prologue registers (16x16, verified): `s` r5 needs NO function-level counter (`i` declared per case
  after the first declaration; with a function-level `Sint32 i`, before or after `s`, `s` is r6); per case
  `Sint32 stride = mc->stride;` first (r5 in cases 1/5, 3/7 with `i` r6), the words named/declared in ADDRESS
  order (`w0 = p+2, w1 = p+6, w2 = p+10` -> r9, r10, r11), `pitch` declared before `i` and `d` (d r7, i r6)
  and assigned after the dst load (pitch itself stays r5, target r3).

**Residues (exact classes, all in the scheduler/allocator, no semantics left open):**
- R1 pre-RA scheduler tie-breaks: priority is DAG height (loads with the longest consumer chain first, IV
  updates and `dcbt` treated as memory-ordered), but the target picks e.g. `addi p0+2` before `add a2+b1`
  (8x8 4p) and `srwi sh0` before `add t0` (8x8 H2, both H=3) where ours picks the other; raw order changes
  (statement order, temporaries, intrinsics, `#pragma scheduling 603/604/750/7400/7450`) never flip these
  two. Best 8x8 shapes: 4p 0.76 blind similarity (mc7: `a0 = s0[0]; b0 = s1[0]; __dcbt; a1 = ..` rolling
  loads + `#pragma opt_propagation off`), H2 case-0 loop body identical except that one `add t0` (h2c).
- R2 register assignment: 8x8 H2 uses NO callee-saved register in the target (11 volatile for 11 live
  values) while every C shape spills 2 (`stwu; stw r30/r31`) — the greedy colouring order differs; the
  target's order (x1 r0, stride r3, w0 r4, a0 r5, d r6, s r7, w1 r8, a1 r9, x0 r10, m1 r11, m2 r12) does not
  follow from any declaration order tried (24 orders, block-scoped, inlined-helper parameters). 16x16 1p 61w
  left: case 2/6 `pitch` r3 (mc dead) vs r5 and `h0` r4 vs the `srwi` temp r0, cases 1/5 & 3/7 `w0` r4 vs the
  `w2 >> 24` temp r3 (a propagated single-use load outranks the later temp in ours), `li i, 0` before the
  stride load in the target's preheader (`stride` declared in the loop body reloads it — 0x678).
- R3 the `mr` copies of the merged OR chains (conservative coalescing at degree >= 29): 16x16 H2 has them on
  all four `a_i`, ours on two (H2 0x468 vs 0x470); follows R1/R2.
**Applied:** src/lib/mpv_mcy.c = pure C (all four kernels; sizes 4p/V2/1p exact, H2 -8 bytes; 61+136+225+263w,
NOT Matching, objects.py untouched); src/lib/mpv_mc.c: header comment only (asm kernels kept, unit IDENTICAL,
111/111 OK). The 8x8 C candidates were not committed (not identical).

### CRI pass 16b: block splitting, the pass pipeline and the size gaps (sfx_cnv Matching; adx_bsc 31 -> 34/36 + .text/.rodata/.data sizes; sfd_adxt 22 -> 23/28; pure C, no pins; 2026-09-11)
Harness ~/.cache/cri16b/ (deleted): `bld.sh lib/unit` (exact ninja command + bytecmp), `fd.py`
(fdiff without ninja), `tryvar.py lib/unit variants.py FUNC.. [--show L] [--keep L]` (text-replacement variants,
~0.3 s each), `passes.sh SRC FUNC` (which backend passes the mwcc-debugger recorded for a function), a
mk-deception clone (its adx_bsc gave two facts: `key_text[16]`, `skg_signature[12] = "CRI-MW"`, and the
4-argument `pl2encodefunc(decoder, *left, left, right)`). tools/research/mwccdbg reused; every claim below was
read off a dump or a variant build.

**Read off the dumps (verified):**
- **The backend splits a basic block before a statement once the block holds > 100 instructions** (adx_bsc
  `ADXB_DecodeHeaderAdx`, backend-00: B18 112 / B19 103 / B20 83 instructions, `:{4000}` flag). The split
  points are visible in the bytes: a `Sint16` variable whose value crosses the split gets its `extsh` there
  (`extsh r30, r12` after the 7th key step, `extsh r12, r12` on the third seed: the peephole removes an
  `extsh` after `lha` only inside a block), a store placed after the split cannot be hoisted above it by the
  scheduler, and a frontend CSE temp first used after the split is loaded after it (the 8th factor `lbz
  0x3b(r1)` late). Consequences used: `Sint16 k` for the chain variable (1009 -> 556w), the first two key
  stores written after `k = skg_prim_tbl[0x300]` (they land in the third block: `extsh; sth k0; mullw; sth
  km` = the target's fill pattern; two `Sint16` result locals — a third local pushed the helper over the
  auto-inline size and it stopped being inlined).
- **`if (helper(..) < 0) return -1;` with a constant-returning inlined helper is folded by the frontend;
  `err = helper(..); if (err < 0) return -1;` keeps `li r0, 0; cmpwi r0, 0; bge`** — the backend's constant
  propagation replaces the operand but folds only the eq/ne branch (pass 14b's `!= 0` case). The
  key-selection if/else chain of DecodeHeaderAdx is `static Sint32 adxb_SetKey(adxb, major, minor, nsmpl,
  k0, km, ka)` with one `return 0` at the end (arms that `return 0` each give a `li` per arm, V1); its
  parameters put the `lbz minor` and `lwz total_nsmpl` loads in the first block (the target's hoisted loads).
  `SKG_MakeKey` clears the three keys through the pointers after the init check (the AHX path's `sth r0`
  zeros; the non-AHX path promotes them to registers and drops the dead stores).
- **The backend chooses its pass list per function**: the mwcc-debugger records CSE / constant-propagation /
  load-deletion for some functions (ADXB_ExecHndl, ADXB_GetOutBps, DecodeHeaderAdx: the "-O4" breakpoint
  group), loop passes + a POST-regalloc CSE for others (ExecOneAdx: `addi r0, r24, -1` after `addi r23, r24,
  -1` -> `mr r0, r23`), and none for EvokeDecode / ADXB_Stop / every small probe (13 passes). Probe f12: a
  reload of the same address in another block (`if (s->d != 0) o = s->d;`) is enough to bring the CSE pass in;
  a loop, calls, an if/else chain, a switch are not. The target's EvokeDecode has the post-RA CSE copy (`mr
  r6, r8` = `blksmpl - 1` reused for `pad`), ours never gets the pass — the trigger for that function is
  unknown (OPEN, 22w left).
- **EADDASS operand order**: `o += X` emits `add o, o, X` for every leaf/expression rhs tried (load, variable,
  cast, substituted local, `-= -X`, `= o + X`); only a SUM rhs `o += A + B` emits `add o, A, o; add o, B, o`.
  `o = X + o` is range-split (a new register, the `rest = o` copy propagated). The target's `add ofst, x70,
  ofst` with the copy kept (ExecOneAdx 1w, EvokeDecode) is therefore an unsplit two-def `ofst` updated with
  the rhs first — no C spelling found (30 forms incl. nested assignments, `register`, Uint32, zero terms
  (folded by the frontend, or kept as a second add when a parameter)).
- Frame layout of scalars/arrays (DecodeHeaderAdx, 0x60 frame): objects are laid out by SIZE class
  descending from the top (16-byte `str[16]` x2 in inlining order, 8-byte `key[4]`, 4-byte `idly[2]`/`idly2[2]`,
  2-byte `hdrlen`, 1-byte `major`/`minor`), within a class in declaration order top-down. `ADXPD_SetDly`'s
  delays are `Sint16 [2]` arrays (mk-deception's `delay_left[2]`).
- ExecHndl: `nsmpl` coloured before the subtraction operands = the operands are OWN locals declared after it
  (`nsmpl, nbyte, cur, last`; `last = cb_nbyte; cur = dec_nbyte;` in that load order), not the frontend's CSE
  temps of `dec_nbyte - cb_nbyte` (which outrank every own local). `ADXB_Stop`: `pl2resetfunc(adxb)` takes the
  handle (the `lis r4` skips the live r3).
- ExecOneAdx / EvokeDecode: `pad = (blksmpl - 1) - ofst % blksmpl` recomputes `blksmpl - 1` (the post-RA CSE
  makes the `mr r0, r23` copy in ExecOneAdx); `nblk2 = ofst / blksmpl` as a single-use local is substituted
  past the GetNumBlk call and keeps the `mullw div, nch` operand order; `pos -= bufsmpl` in place (no `over`);
  the two stereo copies through `static adxb_CopySmpl(dst, src, n)` (dst r4 / src r5), the mono copy written
  out with `pcm` (the helper's dst copy is not coalesced with the dying pcm); `for (i = 0, n = 0; ..)` for the
  `li i; li n` order; declaration order chofst, bufsmpl, pos, pcm (level 2: r31..r28 above adxb r27), pad, nch,
  blksmpl, ofst, i, n, pd (level 1: r26..r23, the loop reusing r23..r25). EvokeDecode's arms in a `void *obj`
  helper (`ADXB adxb = obj`: the frontend CSE no longer merges the arms' `adxb->wr_pos` with the caller's `pos`
  — the target reloads them).
- **sfx_cnv (Matching)**: the LUMI table loop is `static sfxcnv_MakeLumiTbl(Uint8 *tbl)` defined before
  SFX_MakeTable (its 1.164f literal @206 and the int->float 0x43300000 constant @208 are created before
  MakeTable's strings @367/@504 — the .rodata order; the inlined helper's `i` is a @temp, so the backend CSE
  shares the guard's `li r0, 0` with the 16 zero stores and `tbl`/`i` take r3/r4) and the conversion is
  `(Uint8)(1.164f * (Float32)(i - 16))` WITHOUT the `(Sint32)` cast (the fctiwz FPR/slot order of the 8x
  unrolled loop; the cast form was the pass-8 "M1"). The same helper does NOT help sfx_zmv's
  `MakeOrgZ32TblByCCIR` (74 -> 99w; its loop is already cast-free: the target computes the eight `i - k`
  up front — a different DAG, OPEN).
- sfd_adxt `SFADXT_Pause` (12 -> 0): the frame step written out in case 2 (`sfadxt_GoNextFrame` removed) so
  `total` is an own local below `wk` (as a helper local it outranked wk: wk r29 / adxt r30), `tscale` declared
  before `ncount` (frame 0xc/0x8). `SFADXT_Create` 20w: `adxt` is the inlined `sfadxt_CreateAdxt`'s @ret
  (r40, above wk r36); written-out forms change the control flow (28w) — OPEN.
- adx_bsc data: `.data` 8 = an 8-byte zero-initialised static (`skg_dmy[2]`) kept only because a (dead)
  function references it (strip_unused counts references from dead functions in MWCC units); `.rodata` +0xd =
  the literal `"CRI-MW"` (@1118 in Bio4.sym, 4-aligned after the two error strings) in a dead function
  defined after DecodeHeaderAdx (`ADXB_GetSignature`); the target's `...rodata.0_80228808` (0x2d) is our
  `skg_version` — a name note only.
- Hazards this pass: `s.index()` on a function NAME finds the prototype first — two source files were
  duplicated/destroyed by `s[:a] + s[b:]` slices with b < a (restored from the backup / `git show HEAD:`);
  compiling a 2.29M-line file hangs mwcc (killed). bld.sh's eval swallowed compile errors once (a stale
  object then reported the previous word count): check `rror` in the output.

**Applied:** sfx_cnv (Matching, 111 OK), adx_bsc 31 -> 34/36 (DecodeHeaderAdx 1009 -> 0, ExecHndl 7 -> 0,
Stop 2 -> 0, ExecOneAdx 212 -> 1, EvokeDecode 94 -> 22; .text 0x1888 = target, .rodata pad, .data OK),
sfd_adxt 22 -> 23/28 (Pause 12 -> 0). Not reached: adx_dcd5, cri_cvfs, adx_baif, dct_ac, cftfx (its .text
order is already the target's; cnvDynamic..UserTable 135w is a structural `mr`-copy shape), sfd_adxt
ExecServerSub/AdjustSync/SetSpeed/ExcludeHdr.

### CRI SWAR kernels pass 2: mpv_mc asm -> pure C (unit now False, split object linked, 111 OK), 8x8 V2 231 -> 73w with case 0 identical, 16x16 V2 263 -> 225w; the average's association and the 1p asm origin (2026-09-11)
Harness ~/.cache/cri_swar2/ (deleted): `try.py lib/unit file.c [Func..] [--sbs Func]` (the unit's
exact compile command cached from `ninja -t commands`, strip_unused, bytecmp with OBJ override, side-by-side
`dtk elf disasm` with labels normalised; never touches build/), `cc.sh probe.c` (CRI-flag compile + disasm),
probe files probe_upd*.c, ~50 source variants mc_*.c / mcy_*.c; tools/research/mwccdbg `ra.py`/`rasum.py` reused
(dumps under the harness). All 8x8 kernels are pure C now (no asm anywhere in mpv_mc.c / mpv_mcy.c).

**Read off the builds / dumps (verified):**
- **The 8x8 1p kernel was inline asm in the original.** Its rows use `lfdux`/`lwzux` (update-form indexed
  loads) and this compiler NEVER emits them from C: `*(T *)(p += stride)`, `*(p = p + n)`, `*++q`, register
  locals, volatile, Uint32 strides, do/while and for loops with a register step (probe_upd*.c: 13 forms) all
  give `add; lwz`/`lfdx`; only a CONSTANT loop step folds (`lwzu r0, 0xc(r4)`, rna_res / sfd_mpv struct
  copies). Case 3/7's row 3 prefetches with `dcbt s, stride` where rows 0-2/4-5 use `2*stride` — a hand
  typo in straight-line asm. Pure C for 1p (kept) can therefore only reproduce the arithmetic: case 0/4 as
  `p += stride; f = *(Float64 *)p` rows, case 2/6 a 2-iteration loop of four hand-unrolled rows (`li 2;
  mtctr`; an 8-iteration loop is unrolled x8), the second word `h1 | (w0 << 16)` (an `or`: the inserted
  operand is not a rlwinm, so the or->rlwimi merge does not fire; `(w0 << 16) | h1` merges), cases 1/5
  and 3/7 hand-pipelined with the original's dcbt placement. 481w, size 0x4b0 vs 0x450 (one `add` per
  update-form load).
- **The byte average's association is decided by the frontend before constant propagation, and the
  masks' KIND decides it.** `T + S + V` with T = `w & a`, S = `(x & 0xFEFEFEFE) >> 1`, V = `x & 0x01010101`
  as literals is rebuilt as `S + (T + V)` in every one of the 12 parenthesisations/orders (V2 8x8: 97-231w;
  `T + (V + S)` gives the target's instruction ORDER with the wrong roles, 116w); the same three terms with
  the masks as LOCAL VARIABLES `Uint32 m1 = 0xFEFEFEFE, m2 = 0x01010101;` (propagated to the same
  `lis/addi` constants afterwards) give the target's `T + (S + V)` = `add sh, m2; add wa, ·` — 8x8 V2 case
  0 byte-identical with the declaration order below, 16x16 V2 loop schedule identical (registers left).
  Pass 1's reading "the macro form IS the target's association" was wrong for both V2s (ours was
  `add t0 + v; add sh + ·`). `t0 += S + V` is distributed by the frontend into `t0 += V; t0 += S`
  (= the 8x8 H2 target's two-add shape, operand order t0 first): the H2 spelling `t0 = w & a; t0 += x & m2;
  t0 + ((x & m1) >> 1)` stays. A two-def `u = S; u += V; d = (w & a) + u` puts the variable LEFT
  (`add u, t0`); the target's `add t0, u` needs the sum as one expression.
- **Own-local colouring order = declaration order, applied**: V2 8x8 case 0 needs x0 (r7) < w0 (r8) < a0
  (r9) < x1 (r12) < w1 (r28) < a1 (r29) [x1 must precede w1, else w1 takes r12]: `Uint32 x0, w0, a0, x1,
  w1, a1, w2, a2;` (231 -> 73w; five orders of the w2/a2 tail give the same, the cases 1-3 copies do not
  follow the declaration order). The prologue load order is the statement order (`s0 = mc->src; s1 =
  mc->src2; d = mc->dst; stride = mc->stride;` with `d, s0, s1, stride` DECLARED first: d r4, s0 r5, s1 r6,
  stride r0 — V2 8x8 prologue identical; 16x16 loads src2 first: `s1 = mc->src2; s0 = mc->src;`).
- **The frontend range-splits a function-scope loop variable into one web per `switch` case** (`@N`
  copies for cases 1-3; the case-0 web keeps the name; `register`, switch-block-scope declarations,
  do/while loops, `#pragma opt_lifetimes off` (destroys the codegen) do not change it). Consequence for
  the 8x8 H2 R2: the target's loop values x1 r0, w0 r4, a0 r5, w1 r8, a1 r9, x0 r10 with s r7, d r6,
  stride r3, m1 r11, m2 r12 and every `and` in place (r4, r5, r10, r8, r9, r0) is exactly the lowest-free
  colouring of ONE level-2 set {x1, stride, w0, a0, d, s, w1, a1, x0, m1, m2} in that id order, followed
  by the level-1 temps (t0/u0/v0/t1/u1/v1 in place, the count temps r0, the or-merge bases coalesced by
  copy preference) — i.e. the six loop values are single nodes across all four case loops (degree >= 29,
  removed in iteration 2) with the declaration order `x1, stride, w0, a0, d, s, w1, a1, x0, m1, m2`. In
  ours each case's copies are level-1 nodes coloured after the backend temps (which take r0/r3-r12
  first) -> 2 callee-saved. The single-case probe (cases 1-3 deleted) with that declaration order gives
  w0 r4 / a0 r5 / x0 r10 / m1 r11 / m2 r12 and the in-place `and r4 = w0 & a0; and r5 = x0 & m1; and r10 =
  x0 & m2`, confirming the reading; what keeps the webs whole across the switch in the original is not
  found (a use after the switch makes them level 2 but changes the loops: 355w).
- **Mask hoisting (H2 8x8 `lis/lis/subi r11/addi r12` in the entry block)**: 16 spellings of a
  constant-initialised local (`register`, `const`, `Sint32`, `~m2`, casts, Uint16 halves, shifts by a
  runtime zero, `__rlwinm` (keeps a `clrrwi`), `if (0)`/`switch (0)`/`sizeof` dead second definitions —
  all deleted before the propagation decision) are propagated into every loop; only a live second
  definition or `#pragma opt_propagation off` keeps them. Applied: `#pragma opt_propagation off` around
  MPVMC08_OneRefH2_TuneC (448 -> 436w, structure right, registers r6/r3 instead of r12/r11 = the R2 above).
- **4p 8x8 `li r0, 8` / stride r4**: with `__dcbt(s1, stride)` ours colours stride r0 (level 2 first) and
  the count r6; the target's stride is r0-excluded in BOTH dcbt kernels (H2: r3, 4p: r4) and r0 in V2 (no
  dcbt). `__dcbt(a, b)` puts the FIRST operand in rA (r0-excluded) and encodes it first: `__dcbt(stride +
  s1, 0)` / `(void *)stride, (int)s1` give stride r4 + the target's prologue but `dcbt r4, r6` (swapped
  encoding); `s1 + stride`, `&s1[stride]`, `(Uint32)stride`, an `asm { dcbt s1, stride }` on register
  locals all leave stride in r0. Not derivable from the bytes; OPEN (72w: the count/stride swap plus the
  loop's callee-saved permutation r25-r31 vs r27-r31 that follows from it).
- The 4p / H2 R1 ties (pass 1) are unchanged; the V2 schedules follow the association fix (ORDER identical
  in 8x8 case 0-3 first copies and 16x16 case 0 once the association is the target's), so R1 there was the
  DAG, not a tie-break.

**Applied:** src/lib/mpv_mc.c = pure C, all four kernels (4p 72w, H2 436w, V2 73w, 1p 481w; `MATCHING False`
with the per-kernel counts in objects.py; DOL + 110 RELs 111 OK with the split object); src/lib/mpv_mcy.c V2
(mask variables through `MPVMC16_AVG2V`, declaration order `d, s0, s1, stride, x2, x0, x3, i, w1, a1, w0, a0,
w2, a2, w3, a3, x1, w4, a4`; 263 -> 225w, prologue + schedule identical, registers/frame left: 16x16 case 0's
variables are coloured after the cases 1-3 copies that hand out the callee-saved registers). 16x16 H2 with
mask variables: 225 -> 300w (not applied; its association differs — check its target before touching it).

**Residues (exact class):** 8x8 H2 R2 (un-split loop webs, above) + the `and` order R1 inside it; 8x8 4p R2
(stride r0 exclusion) + R1 (`addi p0+2` before `add a2+b1`); 8x8 V2 cases 1-3 (73w: the `@N` copies' order
— target x1 r31 / a1' r28 with the R3 `mr r28, r31` of `(a1 << 8) | a2` because x1 took r31 first — ours
x0/x1 swapped, no `mr`); 8x8 1p (asm origin, not reachable from C); 16x16 V2 225w (R2 as above), 16x16 4p
136w / H2 225w / 1p 61w untouched this pass.

### CRI pass 16a: size gaps first — inlining thresholds, helper-local compares, 32-bit views of Sint64 (mwsfdcre 3 -> 5/10; sfd_mpv 15 -> 24/38; no unit flipped; pure C, no pins; 2026-09-11)
Harness ~/.cache/cri16a/ (deleted): `bld.sh`, `fd.py`, `tryvar.py` as in pass 15 (`--only LABEL`
added; `--fdiff` writes `scratch/<label>.fdiff` with `--all -n 5000`), `mwcc.sh file.c [unit]` probe compiles,
mk-deception (https://github.com/ShulkMaster/mk-deception, `src/libmwsfdg/crimw/dev/sofdec/src/**`, all its
sofdec units NonMatching) used as a SHAPE reference only. Method: align each function region by region with
fdiff, explain every size gap before touching registers. Another agent (`~/.cache/cri_mpv_small`) started on
sfd_mpv's DecodeOneUnit/SkipPic at 11:14 from this pass's file; the DecodeOneUnit and IsSkip flips (-> 25/38)
are its declaration-order edits, not this pass's. InitInf (5w, a register permutation) flipped identical
when only Concat/SkipEndcode changed (K1 variant) — small colouring residues can move with unrelated edits
(an allocation-order tiebreak in the colourer), so judge them on the real build only.

**Mechanisms (each verified by a variant build):**
- **The frontend does not constant-propagate an inlined helper's local**: `static void *MallocWk(MWPLY p,
  Sint32 wksize) { Sint32 size = wksize; return MWSFD_Malloc(p, size); }` called with 0x4000 keeps the
  target's `li r3, 0x4000; cmpwi r3, 0; bge` (mwsfdcre CreateSfd 389 -> 303w, size 0xe70/0xe68). Only the
  wrapper-with-local and struct-member forms work; a helper `const`, a top-level init, a multi-def local are
  folded. Same class: a non-offset-0 struct member is opaque to the folder (offset-0 member folds).
- **Auto-inlining is size-based with a hard threshold**: sfd_mpv GoDdelim in the pass-15 spelling
  (`Sint32 rest; rest = ck1.len + ck2.len; rest = rest - 3; n = rest; n = (n > 0) ? n : 0;` + per-byte
  `Uint8 *r` loop) sits just above it and stays a call in DecodeOneUnit like the target; `int rest` +
  `n = (rest > 0) ? rest : 0` drops below and it is inlined (DecodeOneUnit +0x25c). The threshold is the
  size gap of DecodeOneUnit (377 -> 9w, then identical with the SkipEndcode shape below).
- `int` loop counters fully unroll constant-trip loops with NO zero-trip guard; `Sint32` counters keep
  `li; cmpwi 0x10; bge` (InitInf picusr loops 207 -> 5w).
- An ECOND assigned to a variable of a different type (unsigned -> Sint32) keeps a copy `mr n, r0`
  (GoDdelim, open: 1w).
- **The switch compare tree includes labels whose bodies were emptied/forwarded**: `case 4: ret = FALSE;
  break; case 5: break;` -> `cmpwi 4; beq end; bge end; ...; b end; b end` (IsGopSkip; the backend chains the
  `beq`). A helper returning `Sint32` with `return 0` on the clear path shares the stores' zero register r3
  (SetPicUsrBuf 64 -> 6w).
- A `Sint64` struct field store loads both halves once (`(*inf)->pts = frm->pts`, SetFrmInf identical);
  **a 32-bit view of a 64-bit result is a cast local**: Pts2Tc `m = (Sint32)n; tc->field = m & 1;
  fno = (m >> 1) - tmpref` gives the target's `clrlwi/srawi` on the low word (ours had 64-bit and/shift).
- **`fld % 2` shares the sign temp of `fld / 2` only when computed in the same block**: `field = fld % 2;`
  right after `f += fld / 2;` (before the drop-frame if) gives the target's early `clrlwi/xor/subf`
  (DoReformTc 120 -> 75w, Concat 128 -> 90w); a `type` local (`type = ttu1->tc.type; rnd =
  sfmpv_fps_round[type]; ... tc.type = type`) keeps it in a proper volatile (target r8) instead of r0.
- Concat's tail: `if (t < 0) { ret = -1; } else { if (t > 0) { UpdateConcatTime; nconcat++; } InitTtu x2;
  dlmmask = 0xC0; ret = 0; } if (ret == -1) return -1; SkipEndcode; return 0;` = target `li r3, -1; b chk`
  / `li r3, 0; chk: cmpwi r3, -1` (nconcat++ is inside `t > 0` — semantics differ from pass 15's spelling);
  SkipEndcode as `for (;;) { Get; if (len != 4) break; if (CheckDelim != END) break; Put; AddRtot; }
  Unget;` removes two dead `b`; `tunit = tim->ttu0.unit;` read before `tim->x1f0 += smpl` hoists the load
  over the store; `tc.frm2 = 0` (word at +0x18) and no `tc.x1c = 0`; Tc2Time/ReadTotSmplQue out-params
  declared `tscale, ncount, unit, smpl` (later-declared address-taken scalar = lower slot).
- `void mwSfdDestroy(MWPLY obj) { MWPLY mwply = (MWPLY)(MWPLY_OBJ *)obj; ... }` = the 7 inlined
  `mr r30, r29` of CreateSofdec (551 -> 139w, size 0x1078 matched; pass-12 kept-copy rule).
- A struct-copy address kept in a register needs a pointer local with >= 2 uses (`tot`, DecodeOneUnit END
  block); a `void **pbuf` parameter makes the buffer load happen after `SFTIM_InitTtu` (InitFrm); `frm =
  mpv->frm` + `frm + 2 + i` strength-reduces ChkBufSiz's second InitFrm loop (463 -> 191w, size matched).
- ExecServerSub: `(Uint32)ck.len != 0` = target `cmplwi`; the address-taken scalar slots follow first-use,
  not declaration (24 permutations of ck/sj/wcnt/rcnt never match; 28 -> 22w).
- **Pooling residue (open, recorded):** Pts2Tc's three .rodata tables are addressed with separate lis/addi
  pairs in the target while deferred codegen pools them (`...rodata.0` base + 0x38/0x5c/0x7c). Probes:
  `static const`, struct-typed tables, `extern` first + definition after the function, `#pragma pool_data
  off` around the DEFINITIONS all pool under `-inline auto,deferred`; the same extern-first/define-after
  file WITHOUT deferred does not pool (lis/addi per table = target); `#pragma pool_data off/on` around the
  FUNCTION reproduces the target's shape (81w left: `addi r0; mr r9, r0` ECOND copies) but is a pin — not
  applied. So the original's Pts2Tc was compiled with the tables not yet defined (or non-deferred); the rest
  of the unit needs deferred (.text order, later helpers inlined). Concat/DoReformTc reference one table
  and show no pool either way.
- **The compare of `if (t < 0) .. if (t > 0)` is CSE'd across the arms in ours** (one `cmpwi` + `bge` +
  `ble`); the target keeps two `cmpwi r4, 0`. Helper split (`ret = ConcatSub(sfd, t)`), `ret = -1; if (t >=
  0)`, `t != 0` do not block it (open, 1w).

**Residues (exact class):** mwsfdcre — MallocCompoWork (69w) mwply parameter ranked above the pool temp;
CreateSofdec (97w) lw–vfreq interference (target vfreq r25 / sfdhn r24 / npool r23, 9 callee-saved) + picusr
nskip/buf swap + mode/bps temp swap; CreateSfd (303w) pool order of .rodata/.bss references, register
permutation, two dead `b` of the inlined IsUseAdxt (emptied arm needs a dominating `li` + a case-5-like
label; the A4 form gives the tree but hoists `li r0, 1`); MallocRfb (32w) `blt fail; bge ok` + w16/h16 swap;
CalcWorkSfd (30w) mode/bps order. sfd_mpv — Destroy/Create parameter vs pool-temp rank; ExecServerSub slot
order + mpv/bufin colours; DecodeFrm (55w) single-use `ttu3` struct-copy source folded, AddDecPic arg
schedule; ChkBufSiz (191w) geometry register order, nfrm load slot, `frm` folded into the IV init; Seek (28w)
ck.data address colour; SetFrmPara (27w) geometry temps; GoDdelim ECOND copy; Pts2Tc (96w) pool + `tbl`
colour; DoReformTc (75w) / Concat (90w) arithmetic-block permutation (declaration sweeps of the six block
locals are flat at 75/90) + the un-CSE'd compare; DecodePicAtr (454w, +8) 64-bit ECONDs kept in registers
in the target (`beq; b; mr; mr` pairs) where ours stores/reloads through `mpv->xe78`, untouched.

### CRI paired-single kernels pass 2: MWCC 2.4.7 has no paired-single intrinsics; mpv_umc PpicSkipMb asm -> C, Forward/Backward/BiDirect 0w (11 -> 14/16), OneReadMb 91 -> 72w, Intra 35 -> 16w (2026-09-11)
Harness ~/.cache/cri_ps2/ (deleted): `try.sh unit file.c [FUNC]` (CRI-flag compile of any source to a
scratch object + `bytecmp.py` with `OBJ=`), `tryvar.py unit variants.py FUNC [--base file.c]`, `fd.py unit FUNC [obj]`
(side-by-side objdump target | ours), `pc.sh file.c` (compile + dtk disasm of a probe); tools/research/mwccdbg reused
(NOTE: tools/fdiff.py ALWAYS rebuilds and reads the tree object -- it ignores `OBJ=`; use bytecmp `<unit> FUNC` or
fd.py for a variant object).

**The compiler question, settled by probes (do not re-test):**
- MWCC 2.4.7 build 108 (GC/2.7; also 2.6, and every 3.0a/Wii build we have) has NO paired-single C intrinsics.
  `__PSQ_L/__PSQ_LU/__PSQ_ST/__PS_ADD/__PS_MERGE00/__PS_SEL...` (any case) compile as implicit `int` functions (`bl`
  + the 0x4330 int->float conversion); the compiler binary's only `PSQ_*`/`PS_*` strings are the inline assembler's
  mnemonic table; its builtins are `__abs __fabs __fnabs __frsqrte __alloca __cntlzw __lhbrx __lwbrx __sthbrx
  __stwbrx __dcbf __dcbt __dcbst __dcbtst __dcbz __mulhw __mulhwu __divw __divwu __fmadd(s) __fmsub(s) __fnmadd(s)
  __fnmsub(s) __fsel __mffs __fres __setflm __sync __isync __eieio __rlwimi __rlwinm __rlwnm __memcpy __strcpy`
  (no mtspr/GQR access).
- `__vec2x32float__` IS a first-class C type with operators: `a + b`, `a - b`, `a * b`, `a * b + c` (ps_add/ps_sub/
  ps_mul/ps_madd with fp_contract), `*(__vec2x32float__ *)p` loads/stores -- but every load/store is `psq_lx/psq_stx
  ... 0, qr0` with the offset materialised in a register (`li r0, 8; psq_lx f0, r3, r0, 0, qr0`; never the
  displacement form, never `psq_lu/psq_stu`, never another GQR), unary minus is an internal compiler error
  (Operands.c 635), `__fsel`/`__fabs` on it are "illegal operation", and there is no merge/sum/sel spelling. Every
  kernel here uses quantised GQR3/4/5/7 stores, update forms, ps_merge and ps_sel: NO C spelling exists for them in
  this compiler. The asm bodies of mpv_umc `mpvumc_BiMakeMb/OneMakeMb/OutputIntra6blk/MPVUMC_SetGqr` (mtspr),
  cftyp422_ppc `cnvDynamic/cnvStaticYcc420plnToArgb8888` and dct_fsri `DCT_FsriTransCore`/`DCT_FsriSetGqr` are the
  final source form. (The cftyp422 converters carry compiler fingerprints -- spill slots, a .bss pool base, GQR
  save/restore, psq_st register saves -- so CRI built that unit with a compiler that had intrinsics; the 3.0a3/3.0a5
  builds we have make its C functions far worse (0/8), so nothing to switch to.)
- Inline asm and the compiler: with `#pragma scheduling` ON, asm instructions are scheduled but a C statement never
  moves INTO an asm block's instruction stream: `do { asm {..stores..} } while (--cnt > 0)` gives the target's `li 4 ..
  subic. cnt, cnt, 1; bgt` loop shape exactly (`for (cnt = 4; cnt > 0; cnt--)` is fully UNROLLED, 4 copies), but the
  `subic.` lands after the asm's last store, where dct_fsri's target has it between the last two stores (both
  loops) -- so its counters/pointer steps stay inside the asm (dct_fsri kept exactly as pass 9 left it, Matching
  before/after; 111 OK). `for (;;) { asm{}; if (--cnt <= 0) break; src -= 54; dst -= 0x6c; }` gives the target's
  `subic.; ble; subi; subi; b` tail.

**mpv_umc 11 -> 14/16 (132 -> 88w), pure C, no pins:**
- `mpvumc_PpicSkipMb` (359 asm lines) is C: `p = ref->cpitch / 8` (doubles per row, `srawi; addze`), `if ((ofs[0] &
  0x1f) == 0)` the dcbz path, else plain; four/sixteen macro-unrolled steps `__dcbz(d, 0); a = s[0]; __dcbz(d, p * 8);
  b = s[p]; s += p * 2; d[0] = a; d[p] = b; d += p * 2` (chroma: two rows per step, `s[p]` = `lfdx s, p<<3`, the
  `p*8`/`p*16` shifts CSE'd once) and `__dcbz(d, 0); a = s[0]; b = s[1]; s += p; d[0] = a; d[1] = b; d += p` (luma),
  `ofs[0]`/`ofs[1]` re-read per plane (the target reloads them after the stores), `s` assigned BEFORE `d` in every
  block, declaration order `p, s, d, a, b` (p r9 above d r10; `s, d, .., p` swaps them), and the scheduler ON (the
  interleaved `slwi` pitch shifts and the last row's swapped `lfd f1; lfd f0` are the scheduler's; the asm
  transcription had needed `scheduling off`). `__dcbz(base, offset)` emits `dcbz base, rOff` / `dcbz r0, base` for 0.
  The dead final `s +=`/`d +=` of each block are deleted by the compiler.
- Forward/Backward/BiDirect 2w -> 0w: the output block pointers are a struct `MPVUMC_OUTBLK { Sint32 ccnt; MPVCMC_REF
  rt[6]; }` at 0x120 and the wrappers write `ob = &mpv->outblk;` AFTER the OneReadMb call (the block of the stores)
  and store through `ob->rt[i].p`, passing `ob` to the kernel: add-propagation (same-block rule, pass 14b) folds the
  stores back to `0x124(mpv)`.. and the `addi r4, r31, 0x120` DEFINITION stays where `ob` is assigned -- before the
  `mr r3, wk` argument move in the RTL, which is the target's order of the two hoisted argument setups. A one-use
  `(MPVCMC_REF *)&mpv->ccnt_rt` argument or an `rt` local used only as the argument is propagated into the call (r3
  first). Rule: to put an address argument's `addi` ABOVE an earlier argument's `mr`, make it a pointer local defined
  in the stores' block and used by the stores too.
- `mpvumc_OneReadMb` 91 -> 72w: declaration order = the target's callee-saved order `cpitch r31, ypitch r30, cpos r29,
  ypos r28, mc r27, fn_c r26, fn_y r25, yhx, chx` then `src, cvy, cvx, vy, vx, mby, mbx, tbl_c, tbl_y, mcflag` (the
  tail changes the volatile temporaries: 79 -> 72w), and `chx = cvx & 1; yhx = vx & 1; chx &= mcflag; yhx &= mcflag;`
  (two definitions: the `and`s are emitted before the first call and one callee-saved register fewer -- `stmw r21`
  like the target; the one-definition `yhx = (vx & 1) & mcflag` is propagated into `src + ypitch + yhx` after the
  second call with `mcflag` kept in r28 across both calls). Residue 72w = register naming from ONE frontend
  difference: the target computes the kernel-table index `clrlslwi r12, vx, 31, 2` straight from `vx` and yhx's
  `clrlwi r24, vx, 31` separately, then `and r24, r24, r8` in place (one node, r24 above chx r23 / rfb r22 / dst r21);
  ours CSEs `vx & 1` between the index and yhx (`@184`), so yhx's first definition is a temporary and the two-def
  variable is range-split, coloured below the parameters (r21). `(Uint32)`, `% 2`, `(x << 31) >> 31`, `!= 0`,
  `(vx & mcflag) & 1`, `vx & (mcflag & 1)`, statement order before/after the table lookups: all still CSE (86-91w).
- `MPVUMC_Intra` 35 -> 16w via `ob`. Residue (dump-read, exact): the target's yofs is an UNPROPAGATED one-definition
  node (`slwi r8, r6, 4` mbx16; `mullw r7, r7, r10` y16*yp; `add r7, r8, r7` yofs in place of the dying rB; `lwz r6,
  0x29c; add r6, r6, r7`), and the mpv copy is coloured first (r5: degree >= 29 in iteration 1). Ours propagates
  `yofs` into `pln2 + yofs` whatever the spelling (casts, `&pln[yofs]`, operand order, `Uint32`, an inline helper
  with `yofs` as parameter) and reassociates it to `(y16*yp + pln2) + mbx16` (16w: r6/r7, r8/r9 swapped temporaries);
  the two-definition `yofs = mbx*16; yofs += y16*yp` keeps it a node but MERGES mbx16 and yofs (one node fewer: mpv
  28 neighbours, removed in iteration 1, coloured last -> r10, 33w).

**cftyp422_ppc (2/8, unchanged) -- the mechanisms, so nobody re-tries them:**
- `.rodata 0x88/0x8c` and `cnvStatic` 2w are ONE thing: the target's cnvStatic loads the unit's pooled 255.0f literal
  (`@494` at .rodata+0x50, shared with the table makers' CLIP255) with `lis r5, @494@ha; lfs f21, @494@l(r5)`; inline
  asm cannot name a compiler literal, and a named `static const Float32` is placed AFTER the whole literal pool
  (0x88) even when it is referenced first (the table makers using it instead of `255.0f` also leave it at 0x88). A
  `register Float32 alp = 255.0f` used as the asm's f21 operand does pick f21 (first free callee-saved FPR below the
  asm's hard f22..f31) but the address temporary takes r14 (every volatile is poisoned by the asm's hard GPRs ->
  `stmw r14`) and `#pragma peephole off` (needed by the transcription) leaves `lis; addi; lfs 0(r)` unfolded.
- `.bss` order (cr_r +4, cr_g +0x404, cb_b +0x804, cb_g +0xc04, y +0x1020, CFT_dummy +0x1420 in the target; ours
  CFT_dummy +4, y, cb_g, cb_b, cr_r, cr_g) = first-reference order in CODEGEN: asm operand references count
  (gqr_save is first in both because cnvDynamic's asm names it), a dead `if (0)` reference does not, unreferenced
  objects follow in declaration order. The target's order is cnvStatic's C referencing the five tables through the
  gqr_save-based .bss POOL (`addi r9, r3, 0x4 .. addi r11, r3, 0x1020`, no relocations) before Init; our asm
  transcription reproduces those addi's as immediates, so the tables' first references are Init's. There is no
  code-free way to reference a symbol from C, and an asm `@ha/@l` reference adds relocations the target lacks.
  Init's 24w are these pool offsets plus i r5 / cr_g-IV r10 (the target colours `i` before the fifth induction
  pointer).
- The table makers' 8/8/9w are prologue interleaving ties (target `lfs; addi cb; lfd; addi cr; lfs; li i; lfs; lis`
  alternating FP loads and the `tbl + 0x1000/0x2000` bases; ours groups the addi's first) -- LICM/CSE creation
  order, no shape found. `CFT_Ycc420plnToY84C44` 179w: identical instruction stream, callee-saved permutation (d r26,
  y0 r27, y1 r25, y2 r23, y3 r12 volatile, yskip r21, dskip r22, hblk r24 in the target); its dump has the second
  loop's fourteen >= 29-degree values (c, crp0..3, cbp0..3, cbv, crv, ccnt, o1..o3, i) removed one at a time as spill
  candidates (cost order, not id order) -- not a declaration-order problem, left.

### CRI pass 17b: the post-RA CSE trigger, addi-off-addi, and anchored single-use locals (mpv_cmc Matching; mpv_umc 14 -> 15/16, Intra 16 -> 0; adx_bsc EvokeDecode 22 -> 12w; OneReadMb 72 -> 69w; pure C, no pins; 2026-09-11)
Harness ~/.cache/cri17b/ (deleted): cri16a's `bld.sh`/`fd.py`/`tryvar.py`/`mwcc.sh` copies, `passes.sh
SRC FUNC..` (the backend pass list the mwcc-debugger records per function), a sparse clone of
github.com/ShulkMaster/mk-deception (`src/libmwsfdg`: their mpv_cmc/sfx_zmv/... carry the same residues as ours --
decompiled style, no structure information). tools/research/mwccdbg reused; every claim below was read off a dump or a
variant build (~0.3 s each).

**Read off the dumps / variants (verified):**
- **The backend picks its passes per function from flags set while the PCode is built, not from a fixed -O4 list**
  (mwcc_debugger's breakpoint map: -O2/-O3/-O4 groups, each pass guarded). Probes f1..f6, g1..g3: the loop passes need
  a loop; the PRE-RA CSE (0x500604) is brought in by a same-address reload with the SAME base vreg in another block
  (`if (s->d) o = s->d;` -- a reload through an inlined helper's `void *` copy is a different vreg and does NOT count,
  which is why adx_bsc's EvokeDecode never got it); the POST-RA CSE (0x433EFD, `addi r0, r24, -1` -> `mr r0, r23`)
  runs only when two identical ALU instructions sit in one block with the first result still live at the second --
  i.e. it depends on the PRE-RA SCHEDULE: f1 (`o = a - 1; o += x; pad = (a - 1) - rem`) gets it, the same statements
  with more work in the block (g1, EvokeDecode) do not because the second `addi` is scheduled after the in-place
  `add o, o, x` clobbers the first. In EvokeDecode the target's B1 also holds the `mullw nblk2 * blksmpl` of the `&&`
  condition's second operand: **a single-use local whose only use is the right operand of `&&`/`||` (another block) is
  NOT forward-substituted** (`n2b = nblk2 * blksmpl; if (nblk < nblk2 && pos + n2b - pad < bufsmpl)`), which puts the
  mullw at the block's end and changes the bottom-up list schedule of everything above it; with `x70 =
  adxb->wr_x70` loaded in the statement before `ofst += x70` (its load created after `ofst = blksmpl - 1`, ExecOneAdx's
  shape) the pad `addi` lands two slots after ofst's and the post-RA CSE fires: 22 -> 12w.
- **EADDASS/in-place adds always emit `add o, o, X`**, also for `o = (x = load) + o` (nested rhs, 12w = same code):
  the codegen puts the destination operand first. The target's `add ofst, x70, ofst` (ExecOneAdx 1w, EvokeDecode)
  therefore has ofst != the operand pre-RA: a range-split `ofst = blksmpl - 1; ofst = x70 + ofst` gives exactly that
  operand order, but the split makes `blksmpl - 1` an available @temp and the frontend CSEs pad's `(blksmpl - 1)`
  into it (T live to the pad subf, ofst r7 instead of r8, 29w); the CSE ignores `(Sint32)(Uint32)` casts, `1u`,
  `+ 0`; `x70 + (blksmpl - 1)` and every nested-assignment spelling of it are reassociated to `(x70 + blksmpl) - 1`;
  `pad = ofst` copies (pad 2-def or via a third local) are frontend-propagated. Still OPEN (1w in both functions).
- **Add-propagation does not re-fold an `addi` it has just created**: `ob = &mpv->outblk; oi = ob->rt;` (two addis,
  `oi = ob + 4`) folds `ob` into its ccnt store and into `oi` (`addi oi, mpv, 0x124`) but leaves `oi` as the base
  register of the six stores -- the target's `addi r5, r3, 0x124` (OPEN since pass 1, "M1"). A pointer derived from
  another pointer local is the C form of an unfolded member-array base; one level (`oi = mpv->oi_rt`) always folds.
  Same class as sfd_buf's `ring` (not retried there).
- **Nested-assignment anchors, extended**: the anchored variable is the one whose DEF contains the assignment, and the
  nested variable survives only if used later. mpv_umc Intra: `x8 = (mbx = mpv->mb_x) * 8` keeps x8 (an own local:
  coloured after the backend temporaries -> r9, `add cofs` in place) and makes mbx/mby OWN locals in place of the
  CSE @temps (declared `mbx, mby`: mbx r6 above mby r7 -- as @temps mb_y was created first and took r6); `yofs = .. *
  (ypitch = mpv->out_ypitch)` keeps yofs unpropagated (`add yofs, mbx16, prod` in place, `add pln2, yofs`). 16 -> 0.
  OneReadMb: `yhx = (vx = mv->vec[0]) & 1` stops the frontend CSE of `vx & 1` with the kernel index (yhx r24 / chx r23
  = target, 72 -> 69w); the rest is level membership (cvx/vx/vy have >= 29 neighbours in ours: r0/r6/r4 coloured
  before the temporaries; level 1 in the target: r28/r25/r11). Statement orders move it 65-69w, not applied.
- Inlined-helper single-use parameters (`mpvumc_AddOfs(x, y)`) and single-use `pcm` locals are substituted like own
  locals; a helper-scope `pcm` with a fake second use gave the target's mono-arm schedule (pd loaded after pcmbuf) but
  keeps its `mr r6` copy (pcmbuf r4) -- EvokeDecode's arm residue (10w: pcmbuf/shift r6/r7 in the stereo arms, the pd
  load one slot later in the mono arm) is a scheduler tie between `lwz pd -> mr r3` and `lwz pcmbuf -> add r6` that
  the target breaks the other way; not a declaration/statement-order effect (12 forms).
- sfd_adxt `SFADXT_SetSpeed` 42w: the target addresses its four literals with per-literal `lis/lfs` pairs and no
  pool base; ours pools (also 3 literals in a probe). `-pool off` / `#pragma pool_data off` give the shape (11w, +4
  bytes: one `lis; addi r4, r3; lfs 0(r4)` left unfolded) and leave the other 23 functions identical, but the same
  flag breaks dct_ac (AcIdctDouble, .rodata) / sfd_tst / sfd_cre / sfh_main, so it is not a library flag; the
  function-level condition that disabled pooling there is unknown. Not applied (pragma, not C).
- sfd_cre `sfcre_AnalyMpv` 15w / sfd_tst `SFTST_Calc` 79w / sfx_zmv `MakeCnvZTbl` 96w (src/dst declaration swaps move
  the whole function, 35-110w) / cri_cvfs (four `cvfs_tbl` addis in backend-00, one per inlined search; the
  target's single materialisation after strlen) / sfd_hds `SetHdr`: retried with the new anchors, unchanged.

**Applied:** mpv_cmc (Matching, 111 OK: `MPVCMC_OUTBLK { ccnt; rt[6] }` struct, `ob`/`oi`/`work` locals with `work`
declared first), mpv_umc `MPVUMC_Intra` 16 -> 0 and `mpvumc_OneReadMb` 72 -> 69 (15/16), adx_bsc `ADXB_EvokeDecode`
22 -> 12 (three `void *` arm helpers `adxb_EntrySte/Pl2/Mono` with the out_nch/xdc tests in the caller, `n2b`, x70
loaded at the add; 34/36). Not reached: adx_baif, adx_dcd5, cftfx, cftyp422_ppc, dct_ac, sfh_main.

### CRI sfd_mpv small functions (sfd_mpv 21 -> 32/38 with the 16a agent; 9 of the 11 small functions closed; 2026-09-11)
Harness ~/.cache/cri_mpv_small/ (deleted): `bld.sh [SRC]` (exact ninja command + bytecmp with `OBJ=`), `fd.py SYM
[--obj OBJ]` (objdiff side-by-side without ninja, through a one-unit objdiff.json in a scratch dir so the shared object is
never touched), `tryvar.py variants.py [FUNC..] [--keep L]` (text-replacement variants compiled into scratch objects, ~0.3 s
each). tools/research/mwccdbg reused (`ra.py`/`rasum.py`); every fix below was predicted from one dump or the pass-11..15 model and
confirmed in 1-4 builds. Concurrent hazard: an edit of mine was overwritten by the other agent's stale buffer within seconds
(re-read + re-apply; the word count right after applying is the check). The orchestrator committed the tree mid-pass.

**Zero-code (pure C) closes:**
- `SFD_CalcYccPlane` 5w -> 0 (inlined `sfmpv_CalcYccPlane`): helper locals are coloured LAST-DECLARED FIRST (lowest @N = highest
  vid); the target colours ywidth (in place r5), cwidth (r7), h16 (r8), so the declaration order is `w16, h16, cwidth, ywidth`
  with the assignments kept in evaluation order (`ywidth = ..; cwidth = ..;` as statements: initialiser order = evaluation
  order, which changed the schedule when the declarations were reordered with their initialisers, 20w).
- `sfmpv_InitInf` 5w -> 0: (a) `sfmpv_ChkPara` returns -1 for `nfrm <= 0` too (`if (nfrm <= 0 || nfrm > 16)`; read off `ble` to
  the `li -1`); (b) the two reference-buffer tests are a counted loop `for (i = 0; i < 2; i++) if (tbl[i] == NULL) return -1;`
  — fully unrolled by the frontend, its induction pointer is materialised in its own block before the first load (target
  `addi r3, pool, 0x50; lwz r0, 0(r3); .. lwz r0, 4(r3)`; a `void **rfb = tbl` local folds the first load); (c) `para` as the
  helper's PARAMETER (`sfmpv_ChkPara(&sfmpv_para)`): a helper parameter node ranks below the frontend temporaries (para r4,
  the IV r3), while a `SFMPV_PARA *para = &sfmpv_para` local is propagated into a CSE address temp created BEFORE the IV
  (para r3, IV r4); (d) the `sfmpv_InitFrmTbl` loop steps `frm` in the `for` increment (`i++, frm++`; `sfmpv_InitFrm(frm,
  &mpv->ta_adr[i])`): a source pointer IV puts its `addi 0xe0` before the counter compare in the latch and the strength-
  reduced `ta_adr` pointer's `addi 4` after it (`&frm[i]` gives two SR temporaries updated in the other order; both pointers
  as source IVs, `pbuf` as the source IV, locals in the body, `&mpv->frm[i]`: 16-17w). GC/2.6 (the debugger) emits the two
  addis in OUR order for both forms — this latch tie is a GC/2.7 difference, do not trust the dump for it.
- `SFD_SetPicUsrBuf` 6w -> 0 and `SFMPV_Create` 22 -> 17: `sfmpv_SetPicUsrBuf`'s locals `i, n, p, j` -> `i, n, j, p` with `p`
  declared AFTER `j` (p coloured first: r4, j r5 as the copy of the zero, n r6).
- `sfmpv_IsSkip` 7w -> 0: `sfmpv_IsGopSkip`'s switch operand as a variable read through `*(volatile Sint32 *)&mpv->picstat`
  declared before `ret` (ret `li r0, 0`, stat `lwz r3`); a plain `switch (mpv->picstat)` (or a `Sint32/Uint32/(Sint32)(Uint32)`
  local, propagated) makes the load a backend temporary coloured before the helper local `ret`. The volatile re-read idiom
  (sfd_tim pass 10b/14b) is the one C spelling that keeps a single-use load in a named variable.
- `sfmpv_DecodeOneUnit` 9w -> 0 in three steps: (a) `sfmpv_SkipPic`'s `MPV hn` declared AFTER `ttu3`/`vstart` (its two lower-
  vid removable neighbours): hn had 29 neighbours (12 physical + wk/sj/sfd/done/ttu3/vstart + the 11 `*vstart = *ttu3` copy
  temporaries + flow) = level 2 = r30 in ours; scanned after ttu3/vstart it has 27 left, level 1, and takes the freed r25
  (target). (b) own locals `ret` and `tot` (the END arm's `SFTIM_TTU *tot`, hoisted to function scope, assigned in the arm)
  declared BEFORE `mpv`: level-2 colouring order ret r30, tot r31, mpv r29 (declared first, mpv took r30/r31 first). (c) `Sint32
  n = SFMPV_BUFIN(sfd);` as the first statement of the END arm, used in `SFBUF_GetRTot(sfd, n)` inside the `if`: the frontend
  does NOT forward-substitute a single-use load across a conditional (it does across calls), so the `lwz` stays above the
  `bge` like the target's.
- `sfmpv_SetFrmPara` 27w -> 0: declaration order `w, h, w16, ywidth, cwidth, h16, ysize, csize` (own locals: first declared =
  coloured first; ywidth in place r4, cwidth r5, h16 r6, ysize in place r6, csize r7), the products as nested assignments in
  the ref[0] stores `.cb = y + (ysize = h16 * ywidth); .cr = cb + (csize = (h16 / 2) * cwidth);` (anchors them in that block,
  pass 14), and `h = *(volatile Sint32 *)&atr->height;` — as a variable `h` is coloured after the propagated `w` temporary (w r3,
  h r4, the `h + 15` after the w16 chain); plain/`(Sint32)`/two-def/`h = 0`-after forms are all propagated (10w). `h16` computed
  before ywidth/cwidth: 33w.
- `SFMPV_Seek` 28w -> 0: (a) `if (flg == 0 || SFSET_GetCond(sfd, 0x30) == 0) { END|SEQ } else { END|SEQ|GOP }` (the
  `flg && cond` spelling lays the arms out the other way: 4w); (b) in `sfmpv_SeekVhdr` the cached header bytes + length are one
  object: `typedef struct { Uint8 dat[0x200]; Sint32 len; } SFSEE_VRAW; raw = (SFSEE_VRAW *)vhdr->raw; ck.data = raw->dat;
  ck.len = raw->len;` — `raw` is a node (its `stw` source use cannot be folded, the `len` load folds to `0x238(vhdr)` by add-
  propagation) ranked below the `ttu0 = vhdr->ttu` copy's backend temporaries: `addi r7` hoisted above the copy, copy pairs
  r6/r0 (with `ck.data = vhdr->raw` the addi is a later backend temporary coloured first: r0, copy r7/r6, 24w). The shared
  `SFSEE_VHDR` typedef is used by the 16a agent's `sfmpv_DecodePicAtr` and was left alone.
- `sfmpv_ExecServerSub` 22w -> 17 (open): address-taken scalars in frame order `used, flag, code, done, size, rcnt, wcnt, sj, ck`
  (slots top-down in declaration order, target used 0x28 .. sj 0xc).

**Pins (tagged `// COMPILER-DIFF: pin`):**
- `sfmpv_GoDdelim` 20w -> 0: `register Sint32 n, t; t = (rest > 0) ? rest : 0; asm { mr n, t }` — the target computes the
  branchless max into a temporary (`and r0; mr r31, r0`) and DlmOfst's result directly into `n`; ours renames the ECOND temporary
  into n (28 C spellings: `n = ECOND(rest)`, `rest = ECOND; n = rest`, typed copies, if/else, `Max0` helpers with 1-2 returns,
  `n` as the only variable, `n = 0` before). With the asm copy in arm 1, the inlined `sfmpv_DlmOfst`'s 3-return `@ret` copy
  into n is no longer coalesced (`mr r31, r0` at the join, 4w) — so `sfmpv_DlmOfst` is written out in GoDdelim (`else if`
  chain assigning `n` directly; the helper stays for GetActiveSize/NeedSafeDlmRefresh). Mechanism: two copies into the
  multi-def `n` reach RA and only one coalesces; in the target the max copy is the one that survives.
- `SFMPV_Destroy` 21w -> 6: `asm { mr r31, obj; mr sfd, r31 }` (`register` obj/sfd). The target's handle is a level-2 node
  (sfd r31 above the .bss pool r30, mpv r29, hn r28); as the kept `(SFD)(SFD_OBJ *)obj` copy, as `void *obj` + `SFD sfd = obj`,
  or as the plain parameter it has 28 neighbours (hn/mpv removed before its scan: 26 left) and colours after the pool (r30 /
  r28). Residue 6w: the target's first use goes through the copy (`mr r31, r3; lis r3, pool; lwz r29, 0x1fb8(r31)` — obj dies
  at the copy so the pool `lis` reuses r3) while our backend propagates `obj` into the first load and hoists the `lis r4` above
  the copy; an asm `lwz mpv, 0x1fb8(r31)` is rewritten to `(r3)` too (the inline assembler is not opaque, pass 3). The plain-
  parameter form has the target's SHAPE (`mr r28, r3; lis r3; lwz r30, 0x1fb8(r28); addi r31, r3`) with the parameter at the
  bottom (r28) — the target = plain parameter ranked at the top, i.e. one more neighbour (29) that is not in our graph.
- `SFMPV_Create` 17w -> 8: `asm { addi r30, sfd, 0x23a0; mr mpv, r30 }` (mpv above the pool base r29; `mpv` as own local /
  written-out `&sfd->mpv` / typed `void *` copy / hn-first / ret-first orders: 17-41w). Residue 8w in the inlined
  `sfmpv_SetPicUsrBuf`: the target computes `p = buf + siz` AFTER `pu->dat = buf` so p reuses buf's r4 (`add r4, r4, r7`); our
  pre-RA scheduler hoists the add above the dat store (p r4 / buf r9). At the SFD_SetPicUsrBuf site the target hoists the add
  the same way as ours (buf is a callee-saved parameter copy there), so no helper-level statement order fits both (`p` after
  `n`, `buf` stepped as the parameter: 19/68w).

**Open (exact class):**
- `sfmpv_ExecServerSub` 17w: target mpv r31 (the `stat == 2` block's `SFMPV_WK(sfd)`), hn r29, bufout r28, bufin r29, the inlined
  IsEnoughData's mpv/hn/n r27/r29/r27; ours mpv r28, bufout r27, bufin r28, r29/r28/r28. `ret` is r31 in both (`mr. r31, r3`, not
  interfering with mpv). mpv (18 neighbours) must be coloured before the level-1 temporaries = level 2 (>= 29) or a temporary
  with a lower @N than the helpers'; written-out `SFMPV_WK(sfd)`/`SFMPV_BUFOUT(sfd)` uses are reloaded after calls (67-80w,
  the frontend CSEs a load into an existing variable but never across calls into a temp), all 720 declaration orders of
  `mpv, hn, tim, ret, bufout, bufin` give 17-18w. An r31 pin of mpv would poison ret's r31 — not applied.
- `SFMPV_Destroy` 6w / `SFMPV_Create` 8w: above.
- Model notes confirmed this pass: (1) helper locals colour last-declared first, own locals first-declared first, and a
  helper PARAMETER ranks below the frontend temporaries; (2) the level-1/level-2 split can be moved by declaration order alone
  when a node sits at exactly 29: moving it after its removable lower-vid neighbours drops it into level 1 (SkipPic `hn`);
  (3) `*(volatile T *)&x` is the general "keep this single-use load in a named variable" lever (IsGopSkip, SetFrmPara `h`);
  (4) a single-use def is not substituted across an `if` (DecodeOneUnit `n`), only across calls; (5) a source pointer IV in the
  `for` increment vs `&arr[i]` decides the latch order of two IV updates (GC/2.7 only).

### CRI mwsfdcre/sfd_mpv big closer: sfd_mpv 25 -> 33/38 (Pts2Tc, DoReformTc, Concat, DecodeFrm flipped; DecodePicAtr 454 -> 197w, ChkBufSiz 191 -> 181w), mwsfdcre 5 -> 7/10 (MallocRfb, MallocCompoWork flipped; CalcWorkSfd 30 -> 15w; 2026-09-11)
Harness ~/.cache/cri_mpv_big/ (deleted): cri17b's `bld.sh`/`fd.py`/`tryvar.py`/`mwcc.sh` (paths
repointed), variant files `v_*.py`, ra dumps under `ra_*`. Concurrent with `~/.cache/cri_mpv_small` (the small
sfd_mpv functions; its GoDdelim/Seek/SetFrmPara flips appear in the counts above). Method as in 16a: align region by
region with fdiff, explain the SIZE gap first (frame, callee-saved count, dead `b`), then the colouring. No pins in
sfd_mpv; one hard-register pin in mwsfdcre (MallocCompoWork) and Pts2Tc's `pool_data off` (both tagged).

**Mechanisms (each verified by a variant build):**
- **`x op= e` keeps one node, `x = x op e` after a use is range-split.** DoReformTc/Concat's time-code arithmetic:
  `hour = ttu1.hour; min = ..; sec = ..; sec += f / rnd; f %= rnd; min += sec / 60; sec %= 60; hour += min / 60;
  min %= 60;` gives the target's in-place chain (sec r5 / min r6 / hour r7 defined AT THE LOAD, >= 29 neighbours ->
  coloured before the temporaries); `sec = A; min = B + sec / 60; sec = sec % 60` splits `sec` into `@N` (a fourth
  callee-saved register, DoReformTc r28). The frame count is `f` itself (`f %= rnd`, the `f == 0 || f == 1` test
  and `f = 2` on the same node -> `subf r4, r29, r4` in place); a separate `frm = f % rnd` local (21 neighbours,
  level 1) lands after the `lis 0x6666` temporaries. Declaration order in the arm `rnd, field, f, sec, min, hour,
  type` then `tim, tmpref, fld` (type r8 > tim r9 > tmpref r10 = type/tim/tmpref declared AFTER the arm locals).
- **The empty `else { return; }`** at the end of a void function's arm (`if (wk == NULL) {stores} else { return; }`)
  is the second `b end` of DoReformTc (pass 15's kept jump); nothing else (`return;` inside the arm, `!= NULL`
  early return) produces two consecutive `b end`.
- **Constant-known paths are jump-threaded past later tests** (the frontend knows the value on the path): Concat's
  `t = 0` (ttu1 invalid) and the Tc2Time path jump straight to `if (t > 0)` and skip `if (t < 0)`; the target's two
  `cmpwi r4, 0` are therefore two separate `if`s in different regions: `} else { ...; if (t < 0) t = 0; neg: if (t
  < 0) { ret = -1; goto chk; } } if (t > 0) {...} InitTtu x2; ret = 0; chk: if (ret == -1) return -1;` with the
  ReadTotSmplQue failure `t = -1; goto neg;` (CRI uses gotos: sfd_mps, sfd_cre, adx_sje). Same mechanism in
  DecodePicAtr's reform block: `flag = 0; if (d > 0 && ngop && !x57) { if (newgop == 0) goto chk; ...flag...; if
  (flag == 0) goto chk; } SetCond(0x34, 1); reform = 1; chk:` -- the three failing conditions jump INTO the SetCond
  (semantics differ from the old decomp: an unusable GOP time code forces reform mode), and `reform` stays live
  (`mr. r17, r3`) because it is only redefined on one path. `flag`'s `if (t1 > t2 && t1 < t2 + unit * k)` is `k =
  unit * GetCond(0x35); if (t1 <= t2) flag = 1; else if (t1 >= t2 + k) flag = 1; else flag = 0;` (two `li r5, 1`,
  the multiply before the first compare).
- **A call placed outside an `if` whose arm ends the block**: DecodeFrm's `SFPLY_AddDecPic(sfd, 1, atr->pic_type)`
  is AFTER `if (mpv->pendfrm == NULL) {...}` (the target's `bne` lands on the `lwz r5, pic_type`; the old decomp
  had it inside). Struct-copy source kept in a register = a function-scope pointer (`SFTIM_TTU *ttu3 = &tim->ttu3`
  declared after `flag` so ttu3 r4 / flag r5) -- pass 16a's `tot` rule, block scope is not enough. `void **pbuf`
  parameter (SetPlane) keeps the buffer load after the two `sth` (a load through a pointer parameter is not hoisted
  over the plane stores).
- **Pts2Tc** (identical, tagged): `#pragma pool_data off/on` around the function (M2, see 16a), `tbl = (rate ==
  29970) ? conv_29_97 : conv_59_94` (the if-form keeps `addi r0; mr r9, r0` ECOND copies), `rnd` declared before
  `rate` (fps_round's `lis` first), `fno = (m >> 1) - tmpref` before `tc->field = m & 1`, and `hour` declared LAST:
  as the lowest id it is scanned before its lower-degree neighbours are removed (34 - 3 params >= 29 -> level 2 ->
  r0); with `tbl, rem, fno, min, ten, sec, frm, sec_tot, min_tot` before it in that order (min r8 before ten r9).
- **64-bit results kept in registers** (DecodePicAtr): `t = pts - (Sint64)tmpref * 90000000 / prate; t = (t > 0)
  ? t : 0; tim->x150 = t;` and `d = (d > 0) ? d : 0` give the target's `beq; b; mr lo, rZ; mr hi, rZ` (the `?:`
  on a Sint64 lowers to the inverted branch pair with the clamp copying the compare's zero register); `if (!(d >
  0)) d = 0;` gives `bne; li; li`. `tmpref`/`prate` are loaded BEFORE the origin test (unconditional in the
  target). `wk->pts_ofst += wk->pts_max + 1` = `add; addi` (the `= a + b + 1` spelling is reassociated to `b + 1`).
- **Frame layout of DecodePicAtr**: the two Tc2Time out-parameter pairs are distinct variables (`ncount0/tscale0`
  for ttu0, `ncount/tscale` for ttu3: two more slots, frame 0xe0 -> 0xf0), and both the aggregates and the
  address-taken scalars are declared in the REVERSE of the natural order (first declared = highest slot): `tc2,
  tc3, tc, ent` (0x88/0x68/0x48/0x38) and `tscale0, ncount0, tscale, ncount, delay_byte, delay, vbvsiz, bitrate,
  unit, t2, t1` (0x30 .. 0x8). Pointer locals `ttu0 = &tim->ttu0` (assigned BEFORE its valid test -> `addi r17`
  before the `lwz`), `ttu1b/ttu3` for the picture time and the `*ttu1b = *ttu3` copy; `bufin = SFMPV_BUFIN(sfd)`
  (Sint32) hoists the `lwz 0x1fc0` above `if (p != NULL)`; `raw = vhdr->raw` (MEM_Copy's r3 also bases the rawlen
  store), `n = 0x200; if (len < 0x200) n = len;` (ECOND with the constant first); `br = bitrate` after the vbvsiz
  block (r19 across the vhdr block) and fresh copies `br = bitrate; vb = vbvsiz;` right before the `inf->` stores
  (one load each, reused as the ChkBufSiz arguments). `ret = MPV_GetPicAtr(); *result = ret; if (ret != 0)`.
- **mwsfdcre**: `mwsfdcre_bufnum < 2 || mwsfdcre_bufsize < fsize || mwsfdcre_bufsize < fsize` (the size test
  written twice -- a macro in the original) = the target's `blt fail; bge ok` off one compare (MallocRfb identical
  with it). CalcYccSize: `Sint32 h16; Sint32 w16;` declared in that order with the assignments after (w16 = the
  higher inlined-local id -> coloured first: w16 r10 / h16 r11 in MallocRfb, r6/r7 in CalcWorkSfd/CreateSfd).
  CalcWorkSfd: `mode` is a block local of its own copy of the size macro, declared AFTER `bps` (bps r0, mode r4; a
  function-level `mode` is coloured first whatever its position). MallocCompoWork: **hard-register pin** `register
  MWPLY mwply; asm { mr r31, obj; mr mwply, r31 }` (COMPILER-DIFF M1) -- the parameter, a `void *` kept copy, and
  every declaration order colour the .rodata pool base first (pool r31 / mwply r28); `register` + `asm { mr mwply,
  obj }` alone does not rank it either.

**Residues (exact class, all read off fdiff):**
- sfd_mpv DecodePicAtr 197w (size -4): one extra callee-saved (ours r16-r31, target r17-r31): the target colours
  `mask` r26 right after hn (level 3?) where ours has it at r21 after wk/prate/tmpref/d; the -1 inits are four `li`
  in the target (pts lo/hi, d lo/hi) vs ours `li r25; mr r26, r25` for the range-split `@d = -1` copy (the split
  copy's halves are CSE'd into a copy, pts's are not); `flag`'s zero shares the 64-bit compare's `li r5, 0` in the
  target (flag = r5; ours `li r0` + `li r5`: the Sint32 0 and the Sint64 0 are not merged) and its valid test is
  `bne ARM; b END` (an empty-else layout: `if (valid == 0) { flag = 0 (no code) } else {...}`) -- every spelling
  (`== 0 {} else`, `!`, switch, goto, `flag = flag`) normalises to `beq END`. ChkBufSiz 181w: the target loads
  `nfrm` (`lwz r11, 0x20(mpv)`) in the early slot where ours places `frm = mpv->frm` (`addi r30`) and its `frm`
  later; the IR position of either assignment does not move the scheduler (nfrm as initialiser / statement / after
  the geometry / repeated expression, frm late: all 181), a hard pin `asm { lwz r11, SFMPV_WORK.para.nfrm(mpv) }`
  places the load but the geometry colours and the loop IVs (i r23 / frm r24 / rfb r25 vs ours r25/r23/r24) stay
  (226w). ExecServerSub 17 / Destroy 6 / Create 8: the other agent's functions.
- mwsfdcre CalcWorkSfd 15w: the size chain -- target `add r3 = vib + aib; addi r0 = +0x20; add r3 = +sjb; addi r0
  = +0x840; add r0 += rfb/tab/adxib (in place); add r3 = +adxwk; addi r3 += 0x4800; add r3 = sib + r3` = two
  alternating nodes; every spelling (13 `+=`, one expression, `size = size + c`, two variables, parenthesised
  groups, `return sibsiz + size`) collapses into one node with the constants reassociated (`addi r6, r27, 0x860`),
  and rfbsiz/tabsiz get r0/r4 (target r4/r5) because that single node is r3. Also `width/height` load order in
  the FRMSIZ macro (target width first; swapping the declarations costs 2w elsewhere). CreateSofdec 97w: the target
  has 9 callee-saved (npool r23 / sfdhn r24 / vfreq r25, lw r30 not reused) and 12 more frame bytes (an 8-byte
  local at 0x20 never accessed); npool/vfreq load order, declaration orders and early loads do not change it; the
  seven inlined mwSfdDestroy FreeAll loops then take r23-r25 instead of r27-r29. CreateSfd 297w: pool bases r30/r31
  swapped (target rodata r31 / bss r30 / data r29; ours bss r31 / rodata r30) -- the pool temporaries are created at
  the function start in a fixed order (data, rodata, bss in ours: `lis r120/r122/r124` in the initial code) that an
  early rodata reference does not change; the extra `b` of the inlined IsUseAdxt (`case 4: break; default: break;`
  gives one; `return TRUE` per case, a `ret` local, `ret = TRUE` init: 2 more words or worse); the rest is colouring.

### CRI pass 18b: hard pins as level-shifters, per-function pool_data, the in-place add destination (adx_bsc Matching 34 -> 36/36; sfd_hds Matching 10 -> 11/11; sfh_main 29 -> 35/36; sfd_adxt 23 -> 26/28; dct_ac AcInit 45 -> 7w + .rodata OK; mpv_umc OneReadMb 69 -> 68w; 2026-09-11)
Harness ~/.cache/cri18b/ (deleted): `bld.sh UNIT SRC OUT.o` (the unit's MWCC flags + strip_unused), `try.sh UNIT SRC
[FUNC]` (bytecmp with `OBJ=`), `tryvar.py UNIT BASE.c VARS.py FUNC [names] [--keep NAME]` (variants = lists of exact text
replacements, ~0.3 s each), `fd.py UNIT FUNC [OBJ] [--all]` (objdump side-by-side with the relocation symbols folded in, no
ninja run -- tools/fdiff.py runs ninja UNLOCKED, do not use it while other agents build). tools/research/mwccdbg/ra.py reused.
111 OK after every flip; tools/bytecmp.py is the judge. Tagged forms are `// COMPILER-DIFF: M<n>`.

**Mechanisms read off the variants (verified):**
- **A hard pin adds a physical neighbour to EVERY node of the function, which shifts colouring levels.** Pinning p r28 / len r29
  in sfd_hds `SFHDS_SetHdr` (+ sfh r31 inside the inlined `SFHDS_IsSfdHeader`, which leaves the standalone copy byte-identical)
  takes `result` (28 neighbours, one level with p/len -> r28 in ours) and `sfd` (27) to >= 29: they are removed one iteration
  later, coloured before the level-1 nodes, and by id `result` (r36) takes the new r30, `sfd` (r32) r27 = the target
  (result r30 / len r29 / p r28 / sfd r27 / sfh r31). A hard pin of `result` itself gives the registers but swaps the two
  prologue `mr`s (an asm copy is scheduled by the pre-RA scheduler -- after the `stw` that reads the parameter -- while the
  allocator's own parameter copies are emitted in colouring order, result before sfd); pin the OTHER values and let the
  allocator copy the one you want at the top. Pass 11's model predicts this: count the pinned registers as extra neighbours.
- **The pin poisons the register for the whole function, also where the target reuses it later**: dct_ac `bss` r31 (the
  int->double `lis r31, 0x4330` reuses r31 after the base dies: pin -> 27w), mpv_umc vx r25 / vy r11 (temporaries take r11
  later: 74w), sfd_cre ofs r6 + b5 r6 (42w). A pin is only safe when the target uses the register for that value alone.
- **sfh_main readers (M4 residue closed, 1w each): pin the DYING BASE, not the loaded word.** `asm { lwz r5, SFH_OBJ.hdr(sfh);
  mr hdr, r5 }` in sfh_GetHdrU32/Ver: the word can no longer take the dying r5 and falls to r6 (r0 = the swap result, r3 =
  the return value, r4 = val). A soft `asm { lwz w, 0(p) }` still takes the dying base (the asm's def and use do not
  interfere), a hard `lwz r6` poisons the `li r6` zero of `*val = 0`. SFH_AnlyElemSmpHz (6w) stays C: `peephole off` also
  disables the pre-RA peephole-forward passes, so the inlined search loses the `lbz r0, 408(hdr)` displacement fold and the
  schedule that fold implies (the target's `addi p; lbz 24(p)` was scheduled as dependent and folded afterwards); spelling
  the folded load in C (`hdr[0x180 + i*0x40 + 0x18]`, a struct view, a dedicated search helper) gives the displacement but
  the lbz then schedules above the addi and the compare operands swap; `scheduling off` as well = 16w; the stwbrx fold
  fires even on an asm `stw` and with the stored value kept live by a pin after it.
- **`add o, X, o` (adx_bsc ExecOneAdx/EvokeDecode 1w, OPEN since pass 17b): asm `add ofst, x70, ofst` on register locals**,
  x70 loaded in C in the statement before. Every C spelling (`o = X + o`, `X += o; o = X`, `-(rem - (b-1))`, `|0`, `-1 +`)
  either keeps `add o, o, X` or CSEs `blksmpl - 1` into pad (36w). **EvokeDecode's arm residue (10w) is the in-place add
  destination**: `pcm = adxb->pcmbuf; pcm += adxb->wr_pos;` makes pcmbuf the destination of the add (`add r6, r6, r7`,
  pcmbuf coalesced with the r6 argument, the shifted offset r7) and moves the pd load below pcmbuf in the mono arm; the
  expression form `adxb->pcmbuf + adxb->wr_pos` coalesces the result with the shift temporary (`add r6, r7, r6`). Pure C.
- **`#pragma pool_data off` around ONE function is per-function** (sfd_adxt `SFADXT_SetSpeed`: the other 27 functions keep
  their pools; `#pragma pool_data on` after the closing brace). The remaining 11w there was the frsp/literal FPR order:
  `l = (Float32)log((Float32)speed); cent = 1731.234f * (l - 6.9077554f);` -- with the log result in a local the frsp is in
  place (f1) and the 1731.234f literal takes f2; the one-expression form ranks the literal first. Pragma + one local = 0w.
- **dct_ac DCT_AcInit .rodata order = a helper's literals** (pure C): `static Float64 dctac_Cos(Float64 w, Sint32 j) { return
  cos(w * (0.5 + (Float64)j)); }` defined before AcInit creates 0.5 and the int->double constant before AcInit's 0.3535/pi/8
  (target @228/@230 vs @288/@289: the 58-id gap IS a separate function). `pool_data off` there also unpools .bss (the target
  keeps the .bss pool: dctac_i_const = the pool base r31, the version dummy at +0x400, dctac_f_const at +0x200), so the
  .bss addressing is asm-emitted: `asm { lis hi, dctac_i_const@ha; addi bss, hi, dctac_i_const@l }`, `asm { addi ip, bss,
  dctac_i_const@l }`, `asm { addi fp, bss, dctac_f_const@l }` with `ip += 8; fp++` as the loop pointers (the asm addi IS the
  IV init; a `(Float64 (*)[8])bss` cast costs a `mr`). 45 -> 7w: the base is coloured r29 (target r31; own local below the
  loop temporaries, 24 declaration orders tried), and the second addi's slot. `const Float64 x = 0.5;` objects are
  constant-folded (no rodata object), so named literals cannot replace anonymous ones in C.
- **A parameter above the locals (sfd_adxt ExcludeHdr data r29, Create wk r30): hard pin at the top**, `asm { mr r29, data;
  mr d, r29 }` as the first statement (placed after the first statements: 7w -- the copy must be coalesced into the
  prologue). `asm { addi r30, sfd, SFD_OBJ.adxt; mr wk, r30 }`: the `STRUCT.member` immediate works in addi.
- **sfd_adxt AdjustSync 69 -> 51w (pure C): `ins = ADXT_InsertSilence(..); n -= ins; wk->smplofst -= ins;`** -- the target's
  `subf r0, r3, r0` subtracts the call RESULT from smplofst, ours subtracted the remaining n (a real bug in our source), and
  the locals declared in REVERSE (ofs .. wk) put the parameters at r23..r26 = target. Residue: skip r30 / skipbyte r29 / nch
  r28 / astart r27 above the parameters and vstart r22 / tim r21 below them in the target; ours tim above, astart/skipbyte
  below (levels, not ids); a tim pin regresses (69w).
- mpv_umc OneReadMb: `mby8 = mby * 8` / `mby16 = mby * 16` as own locals give `mullw r0, mby8, cpitch` (an anonymous product
  is the SECOND operand). 68w left = the schedule of the whole ofs/tbl block (statement orders do not move it) + cvx/vx/vy.
- adx_tsvr `adxt_nlp_trap_entry` 2w: 12 more spellings (ternary n2, Sint32 temp, cast, reorder) keep `lha r0`; the hard r4
  pin displaces the two `lis r4, 0x8000` temporaries (6w), as pass 7 found. sfd_cre AnalyMpv 15w: statement orders of the
  byte loads do not move the schedule; the residue is `ofs + 1` in place vs the target's fresh r0 (target's post-RA schedule
  puts the addi after the dead-r0 `rlwinm.`; ours schedules it 2nd). cftyp422 table makers 8w each = the hoisted
  `addi cb/cr` bases scheduled before the pooled literal loads (target interleaves them after `lfs f9`).

**Applied:** adx_bsc Matching (36/36; two `add` pins M1 + the pcm in-place form), sfd_hds Matching (11/11; three hard pins
M1), sfh_main 35/36 (hdr r5 pins), sfd_adxt 26/28 (SetSpeed pragma M2 + local, Create/ExcludeHdr pins M1, AdjustSync
C), dct_ac 2/3 with AcInit 7w and .rodata OK (helper + pool_data off + asm pool addresses M2), mpv_umc 15/16 68w. Not
reached: sfx_zmv, sfd_tst, adx_baif, cri_cvfs, adx_dcd5, cftfx, cftyp422_ppc Init/Y84C44.

### CRI sfd_mpv/mwsfdcre final closer: sfd_mpv 33 -> 35/38 (SFMPV_Destroy 6 -> 0 pins, SFMPV_Create 8 -> 0 pure C; ExecServerSub 17, ChkBufSiz 181, DecodePicAtr 197 open), mwsfdcre 7/10 (CalcWorkSfd 15 -> 6w pure C; CreateSofdec 97, CreateSfd 297 open; 2026-09-11)
Harness ~/.cache/cri_mpvfin/ (deleted): cri18b's `bld.sh` (now takes the unit's exact flags from `ninja -t
commands`, incl. `-inline auto,deferred`), `fd.py`, `tryvar.py`, `probe.sh UNIT FUNC START END BODY.c [--fd] [--ra]` (one
function body substituted into the tree source, compiled, word-counted, RA-dumped), `pp/` = tiny TUs for the pool-order
probes. tools/research/mwccdbg `ra.py`/`rasum.py` reused; the dump dir numbering differs per run (`backend-NN-...`), grep by
name. No unit flipped; 111 OK after every edit. Note for the tools: `pkill -f <pattern>` kills your own shell when the
pattern is in its command line.

**Closed:**
- `SFMPV_Destroy` 6 -> 0 (two pins): `asm { mr r31, r3; mr sfd, r31 }` reading the INCOMING r3 instead of the `obj`
  parameter, plus `asm { lwz r29, SFD_OBJ.tr[SFMPV_TR].hn(r31); mr mpv, r29 }` (`register` sfd/mpv). Mechanism: a copy of
  a parameter variable (`mr r31, obj`) is propagated by the backend into the first load (`lwz mpv, 0x1fb8(r3)`), which
  keeps r3 live past the copy and pushes the pool `lis` to r4; a copy of the physical register r3 is opaque, obj dies at
  the `mr` and the `lis r3` reuses it. With sfd pinned r31 the pool base still colours before mpv (mpv r30 / pool r29 with
  one pin), hence the second pin. RA dump of the plain-parameter form: sfd 28/28 neighbours (level 1, lowest vid ->
  coloured last, r28); `ret` locals, `void *obj` copies, `mpv->mpv` tests, written-out `sfd->tr[2].hn`: all 28.
- `SFMPV_Create` 8 -> 0 (pure C, the pin of `mpv` stays): in `sfmpv_SetPicUsrBuf` the local `Uint8 *p` (declared last)
  is assigned `p = buf;` as the FIRST statement, the NULL test and every store read `p`, and the step is `p += siz`. At the
  Create site the argument is the global load `sfmpv_picusr_pbuf`: the frontend substitutes the single-use load into `p`
  (one node), so `p += siz` is a WAR dependence on the `pu->dat = p` store and the pre-RA scheduler cannot hoist the add
  (`stw r4, 12(r8); add r4, r4, r7`). At the SFD_SetPicUsrBuf site `p = buf` is a plain copy of the caller's parameter
  and is propagated (stores use r29, `add r4, r29, r31` hoisted) -- both sites from one helper. The copy placed AFTER the
  NULL test is not substituted (pass 16a's "not across a conditional"), and `p = (Uint8 *)buf + siz` after the stores or
  `p = buf` + stores through `buf` are two nodes whose add the scheduler hoists (buf r9 / p r4, 8w). Read off the dumps:
  `backend-11-after-scheduling` showed the pre-RA scheduler moving `add r46, r36, r39` above `stw r36, r42, 0xc`; the
  stepped-parameter form (`Uint8 *buf; buf += siz`) gives the exact shape at both sites but colours j r4 / n r5 / buf r6
  (helper locals outrank helper parameters; a nested slot-loop helper 20w).
- `mwPlyCalcWorkSfd` 15 -> 6 (pure C): two alternating accumulators `size = vib + aib; size2 = size + 0x20; size = size2 +
  sjb; size2 = size + 0x40 + PICUSR; size2 += rfb; size2 += tab; size2 += adxib; size = size2 + adxwk; size += HNWORK;
  size += 0x700; size += FNAME; size += sibsiz; return size;` (`b = a + c` is a new node, `b += x` in place = the target's
  r3/r0 alternation; the frontend keeps size/size2/@939/@940 as in the target). The total MUST be `size += sibsiz; return
  size;`: `return sibsiz + size` substitutes the whole tail into the return expression and the backend then emits
  `EADD(sib, EADD(EADD(x, adxwk), c))` as `add; add sib; addi c` (constant moved to the end, 15w); `size = sibsiz + size`
  merges the same way (7w); `sibsiz += size` adds an `mr` (9w); a nested `(size += c)` anchor, a block-scoped total and
  `register` do not stop the substitution. Residue 6w: the last add is `add r3, r29, r3` (sib first) in the target and
  `add r3, r3, r29` in ours (an `asm { add size, sibsiz, size }` gives it but not the rest), the epilogue `lwz r0, 52(r1)`
  is hoisted 3 slots by the post-RA scheduler in ours (both forms; with the old single-node chain it was not hoisted,
  because r0 was the chain register), and the FRMSIZ `width`/`height` load order (`lwz r23, 8; lwz r24, 12` vs ours 12
  then 8) does not follow the declaration or statement order in a private copy of the macro.

**Open (exact class, what was tried):**
- `sfmpv_ExecServerSub` 17w: target `mpv` (the `stat == 2` block) r31 = coloured before every level-1 temporary (right
  after ret/sfd; ret shares r31 because they do not interfere). Ours: mpv 18 neighbours, level 1, vid below the backend
  temporaries -> r28. A hard pin `asm { lwz r31, SFD_OBJ.tr[2].hn(sfd); mr mpv, r31 }` removes r31 from the allocator for
  the whole function (ret -> r30, sfd -> r28, 70w); pinning ret too via `asm { mr r31, ret; mr ret, r31 }` after the call
  is propagated away (still 70w). Defining mpv before the SetMpvCond block (more neighbours) gives r28 with the SetMpvCond
  reload CSE'd into it (21-55w). The target's mpv must be a top-level (>= 29 remaining degree) node or a late backend
  temporary; a source form for that was not found.
- `mwsfcre_CreateSfd` 297w: **pool-base order, mechanism found, cause not**: the three section-pool temporaries are created
  at function start in the CODE order of the FIRST reference to each section (probe TUs `pp/q1..q22`: `lis r33 bss;
  lis r34 rodata; lis r35 data` for first refs bss, rodata, data) -- NOT declaration order, not TU order (moving the .bss
  declarations, the crepara/vonlysfd definitions or a dummy early rodata reference changes nothing). But the colouring
  among them is NOT vid order: they are top-level (spill-candidate) nodes and their order follows the spill-cost ranking
  (q19 `[cond r] d [b] [r]` -> data r31 / bss r30 / rodata r29, q20 `[cond r] d [r] [b]` -> rodata r31 / bss r30 / data
  r29: the same first-reference order, different colours). CreateSfd target rodata r31 / bss r30 / data r29 vs ours bss
  r31 / rodata r30 / data r29 = the rodata pool ranks above the bss pool in the original's spill-cost order (rodata: 4
  string addresses; bss: ~25 references) -- i.e. the original's bss live range/degree differs (fewer bss references
  through the pool, or more rodata ones). Everything else in CreateSfd is colouring downstream of this. The IsUseAdxt dead
  `b` was not touched.
- `sfmpv_ChkBufSiz` 181w: `#pragma scheduling off` around the function = 243w (the target IS scheduled; the pragma also
  leaks into following functions if the `on` is misplaced). `sfmpv_DecodePicAtr` 197w: `asm { mr r26, mask; mr msk, r26 }`
  = 242w (the pin excludes r26 function-wide and reshuffles everything); the extra callee-saved (r16) needs the
  colouring, not a pin. `mwPlyCreateSofdec` 97w not attempted.
- Model notes confirmed: (1) a hard-register asm pin makes that register unavailable to every other node of the function
  (Destroy/Create/GoDdelim pins work only because the pinned register is the target's for a node live across the whole
  function or otherwise unused); (2) inside an inlined helper the vid order is locals (last declared highest) > parameters
  (first parameter highest) -- a stepped parameter can never colour above a helper local; (3) the frontend substitutes a
  single-use load into a plain copy only when the copy is the next statement (not across an `if`); (4) the backend emits
  `EADD(a, EADD(x, c))` with the constant last -- a variable whose last def is `+= const` and whose single use is in the
  return expression loses its `addi` position.

### CRI pass 19b: the pool base as an own local, `addi rD, rA, 0` needs a relocation, IV-temp creation order, hard-pin schedule costs (dct_ac Matching 2 -> 3/3 + ldscript `_savefpr_27/_restfpr_27`; sfx_zmv MakeCnvZTbl 96 -> 12w; cri_cvfs GetFileSize 63 -> 45w; adx_tsvr 2w / sfh_main 6w / sfd_cre 15w / sfd_adxt 51w read, not closed; 2026-09-11)
Harness ~/.cache/cri19b/ (deleted): `bld.sh UNIT SRC OUT.o` (the unit's MWCC GC/2.7 flags through sjiswrap + strip_unused,
no ninja), `try.sh UNIT SRC [FUNC]` (bytecmp with `OBJ=`), `tryvar.py UNIT BASE.c VARS.py FUNC [--keep NAME]` (`V = {name: [(old,
new), ..]}` exact replacements, ~0.25 s per variant, unit total per line), `fd.py UNIT FUNC [OBJ] [--all]` (side-by-side
`llvm-objdump --triple=powerpc-unknown-eabi -d -r` with the relocation symbols folded in; there is no powerpc objdump in
build/binutils, llvm-objdump from /usr/bin works). tools/research/mwccdbg/ra.py + rasum.py for every ranking question. 111 OK after
the flip; tools/bytecmp.py is the judge (a standalone compile shows `_savefpr_27` UNRESOLVED until the DOL is relinked -- that is
not a diff).

**dct_ac DCT_AcInit 7 -> 0 (Matching), two mechanisms:**
- **The asm-emitted .bss pool base ranked below the frontend's induction-pointer temporaries.** With `ip[j] = v; fp[j * 8] = v` the
  inner loop's row/column pointers are range-split frontend temps `@103/@104` (ids above every own local), coloured before `bss`
  (a `register` local): r56 = the hoisted 0x4330 constant took r31, @103 r30, @104 r29, and `bss` then took the lowest free
  handed-out callee-saved register = r29. Writing the two pointers as OWN locals `p = ip; q = fp; *p = v; *q = v; p++; q += 8`
  declared BELOW `bss` (first declared = highest id) puts `bss` first among the locals: it does not overlap the 0x4330 constant,
  so it takes r31 (the target: the pool base is the compiler's last-created backend temp = highest id = r31, the constant reuses
  r31 after the base dies), p r30, q r29, ip r28, fp r27 -- p/q must be declared BEFORE ip/fp (pq_after gave ip r30 / fp r29).
- **`asm { addi ip, bss, 0 }` becomes `mr`: the backend's constant propagation turns a literal `addi rD, rA, 0` into a copy**
  (every spelling: `la ip, 0(bss)`, `subi`, symbol differences are rejected by the asm parser; `opt_propagation off`,
  `opt_lifetimes off` do not stop it). The target's `addi r28, r31, 0` is the pooler's pool-relative offset, emitted after that
  pass. Fix: give the immediate a relocation whose resolved low half is 0 -- `addi ip, bss, __ArenaHi@l` (`extern Uint8
  __ArenaHi[]`; the ldscript absolute 0x81780000; bytecmp folds an `abs` target into the word and compares it equal to the
  literal 0, the linker writes 0). The pass-18b `addi ip, bss, dctac_i_const@l` / `addi fp, bss, dctac_f_const@l` were wrong
  code (bss is already the full address; the low halves 0xfa18/0xfc18 were double-added) -- the pool offsets are 0 / 0x200.
- NEGATIVE, `pool_data on` route: the compiler's own .bss pool gives `addi 28, 31, 0` and r31 for free, and the rodata pool goes
  away with only two anonymous literal objects left (0.5 x2 refs + cvt: the threshold counts objects, not references), but the
  two named `static const Float64` for 0.3535/pi/8 (defined right before AcInit they DO land at .rodata +0x18/+0x20 = the target
  order) cannot be loaded: an asm `lis a, sym@ha; lfd k, sym@l(a)` under pooling has its `lis` DELETED by the register allocator
  (the lfd keeps `sym@l` with a garbage base -- the pooler treats the asm high half as a pool-base def); a `*(volatile Float64
  *)&sym` read is pooled like any object (4 objects again). Under `pool_data off` the asm lis survives (pass 18b's base).
  Also: loop-code-motion hoists an asm `lfd` out of the loop but not its `lis` when the address register variable is shared
  by two asm blocks (a1/a2 must be distinct); FPR ranking of `register Float64` locals follows the GPR rule (k/c/w declared
  in that order = f29/f28/f27).
- ldscript: `_savefpr_27 = _savefpr_14 + 0x34; _restfpr_27 = _restfpr_14 + 0x34;` (symbol expressions on the split object's
  symbols; the pass-8 note said these would be needed at the first Matching unit saving f27..f31).

**sfx_zmv sfxzmv_MakeCnvZTbl 96 -> 12w (pure C): the frontend creates the induction-pointer temps in the order of their FIRST
USE in the loop body, and a temp created first has the higher id.** `*dst++ = (Uint16)(*src++ >> 15)` / `*dst = *src++ & ..`
create dst's @N first (the store's address is visited before the load) -> dst coloured first -> dst r3 / src r4 (target src r3 /
dst r4 in the two linear loops, src r5 / dst r6 and src r29 / dst r28 in the perspective loops where `if (*src == 0)` uses src
first). Declaring the pair dst-first and ASSIGNING src-first (`Uint32 *dst; Uint32 *src; src = orgtbl; dst = tbl;`) in all four
inlined branches gives the target's pairs; the assignment order matters twice: with the copies in that order `tbl` (the caller's
r31) stays live across the three src copies, which are its 3 missing neighbours (33 -> 30 total with dst copied first: tbl drops
from level 2 to level 1 and takes r29, zmf_dat r31). Left 12w: with assignment statements the two linear loops' copies are emitted
in the loop-guard block (`li 0,0; mr 3,27; cmpwi 0,256; mr 4,31; bf`) where declaration initialisers are sunk into the
preheader after the `bf` (`li 0,16; mr; mr; mtctr`) -- but initialisers can only be dst-first-copied; any multi-statement body
(`v = *src++; *dst++ = v`, split increments) also lands in the guard block; a `void *tbl` helper parameter (kept conversion
copy) changes nothing; plus one `addi 28, 28, 4` slot in the Z32 perspective loop. Hard r3/r4 pins of the pair (pin_lin)
displace the sfxz_work address temps and the r5/r6 copies (40w): the poison rule again.

**cri_cvfs cvFsGetFileSize 63 -> 45w / cvFsOpen 148 -> 152w:** `tbl = cvfs_tbl;` assigned AFTER the default-device block in
the inlined `cvfs_ResolveDev` (an initialiser hoists the `addi tbl, base, 324` above `addi dev, r1, 308` and renumbers
dev/tbl r27/r28 -> target dev r28). The target materialises tbl after the inlined cvFsGetDevIf's strlen (`addi 27, 31, 324; mr
24, 27` = the pass-13 "tbl materialised after strlen"); ours still hoists it above the `bf` of the guard. Without the local
(`cvfs_tbl` in all three calls) the shared copy disappears (114w).

**Read, not closed (mechanisms for the next pass):**
- **adx_tsvr `adxt_nlp_trap_entry` 2w (`lha r4` vs r0).** The three r4 free-choice values of the target (the first call's
  `lis r4, 0x8000`, the second call's vtbl `lwz r4, 0(sji)`, the `lha r4`) CAN all be hard-pinned (`asm { lis r4, 0x8000; mr
  hi, r4 }` + `hi - 1` as the argument; `asm { lwz r4, SJ_OBJ.vtbl(sji); mr vt, r4 }` + `(*vt->GetChunk)(...)`; `asm { lha r4,
  ofst; add ofst1, ofst1, r4 }`, all with `register` locals): registers all match, but the pinned lis's consumer `addi r5, r4,
  -1` is then scheduled BEFORE the argument move `mr r3, sji` (target after it), 2w in a new place. Why: with a C `addi` after
  an asm `lis`+coalesced copy, the post-RA scheduler refills the deleted copy's slot with the addi (its `li r4, 1` WAR successor
  outranks the arg move); with an asm `addi` (or an inlined helper containing the asm) the pre-RA order already has it before
  the arg moves (an inlined helper's body is emitted before the call's argument moves; the asm's own dependency has no
  latency). `asm { mr r4, hi }` and `asm { mr r4, hi; mr hi, r4 }` are deleted as dead copies (no pin: a pin needs a hard
  DEFINITION inside the asm). 20 spellings/placements tried; the r0 neighbour the target had (a value coloured r0 live at the
  lha) has no C source in our shape. Accepted at 2w.
- **sfh_main `SFH_AnlyElemSmpHz` 6w (M4).** `#pragma peephole off` around it: 10w = `clrlwi 9`/`mr 7` order, the two `lbz
  408/472(hdr)` folds lost, and the swap word/result r0/r4 vs the target's r6/r0 (the same "one more r0/r4 neighbour" the
  pass-11 note describes: the target's swap chain was merged after allocation, so its partial results were live); an
  asm-spelled `lbz pid, (0x180+0x18)(hdr)` search copy under peephole off = 39w (the unrolled search loses `addi 6, 8, 384`
  and the `mr 7, 3` counter). Accepted at 6w; the unit stays 35/36.
- **sfd_cre `sfcre_AnalyMpv` 15w:** the whole permutation follows from ONE interference: ours schedules `addi ofs+1` between
  `rlwinm r50` (the `(b7 >> 4) & 0xF` test temp, r0) and its `cmpi`, so ofs+1 cannot take r0 and goes in place (r4), which
  pushes b4 to r6, ofs to r4, b5 r7, b6 r8, b8 r4; the target fills that gap with `lbz b6` (ofs+1 r0, b4 r4, ofs r6, b5 r6
  reusing ofs, b6 r7, b8 r8 -- every colour then follows the lowest-free rule). Seven statement orders (byte loads before/after
  the ofs/size block, b6 before b5, size first, the test moved) leave the schedule unchanged; the target's `lbz b6` outranks
  the addi, so b6 (or the addi) had a different in-block height in the original -- a different statement shape, not an order.
- **sfd_adxt `sfadxt_AdjustSync` 51w is a level problem in both directions:** ours has endflg (32 neighbours) in level 2 and
  astart (23) in level 1; the target has astart in level 2 (r27) and endflg in level 1 (reusing r27 after astart dies), tim in
  level 1 coloured after vstart (r21). Uniform level shifters (hard pins add +1 to every node) cannot lower endflg while raising
  astart; the neighbour sets themselves differ (astart needs +6, endflg -4): a different live range for endflg (e.g. defined
  later or in both arms) is the lever to try.
- mpv_umc OneReadMb 68w, sfd_tst Calc 79w (400+ nodes, three levels, r23/r25 pair swap cascades), sfx_zmv CCIR 74w, adx_baif,
  adx_dcd5, cftfx, cftyp422 (also .bss order + .rodata size) not attempted this pass.

### CRI SWAR kernels pass 3: mpv_mcy 16x16 1p 61 -> 57w (pure C); the 1p target's loop temporaries are coloured in ASCENDING statement order (ours descending) and its hoisted pitch has a late backend id; nothing flipped (2026-09-11)
Harness ~/.cache/cri_swar3/ (deleted): `try.py lib/unit file.c [Func..] [--sbs Func] [--all]` (unit flags compile of a
scratch copy, strip_unused, bytecmp with OBJ=, side-by-side `dtk elf disasm`; `OPT=`/`XFLAGS=` env for flag sweeps), tools/research/mwccdbg
`ra.py`/`rasum.py` for the graphs (dumps ra_1p, ra_v1, ra_v6, ra_v7, ra_4p, ra_v2_8). Units owned: lib/mpv_mc (4p 72, V2 73, H2 436,
1p asm) and lib/mpv_mcy (1p 61 -> 57, 4p 136, H2 225, V2 225). 111 OK before and after; no MATCHING change.

**Applied (pure C, zero code): mpv_mcy `MPVMC16_OneRef1p_TuneC` cases 1/5 and 3/7 assign `d = mc->dst; stride = mc->stride;` in that
order (declarations unchanged, initialisers moved out)** -- the two preheader loads are scheduled in statement order (target `lwz r7,
0x18(r3)` before `lwz r5, 0x20(r3)`), 61 -> 57w, size still exact.

**Read off the target / the dumps (verified):**
- **The 1p target colours the pack temporaries in ascending statement order; ours colours them in descending id (= reverse creation)
  order.** Case 1/5 target: `w1>>24` r0, `w2>>24` r3, `w3>>24` r4, the `b` copy r0, the `w0` load r4; case 3/7 the same cycle r0, r3, r4,
  r0 with `w0` r4; case 2/6 `w0>>16` r0, `h0` r4, `w2>>16` r5, `w1>>16` r4, the `h1` copy r0, pitch r3. Working every lowest-free
  assignment backwards against the (identical) schedule: the target needs `w2>>24` coloured before `w3>>24` (1/5, 3/7) and `w0>>16`
  before `w1>>16` before `w2>>16` (2/6), i.e. d[0]'s temp first, d[16]'s last, with the single-use first-word load (`w0`/`h0`, propagated
  into d[0]'s expression in ours) coloured after the temps. Ours (dump ra_1p: ids h0 138 < w0>>16 140 < w1>>16 143 < w2>>16 146 <
  h1-copy 149; colouring 149, 146, 143, 140, 138) gives the mirror image, and the 8x8 V2 case 0 (identical) confirms descending-id
  colouring for our compiler. So the target's temps were CREATED in the reverse order, or are frontend `@N` temps (created first =
  highest id = coloured first). Not reproduced by: statement order d[0], d[16], d[1], d[17] or fully reversed (the pre-RA scheduler keeps
  stores in statement order, so the stores move too: 69/94w), the packs in variables x3..x0 computed in reverse then stored in order
  (single-use variables are sunk to the stores; only x3 survives because its expression holds the `p[15]` load, which cannot cross the
  stores: 91w; `#pragma opt_propagation off` changes the schedule: 104w), inline loads CSE'd across the four packs (100w), an inlined
  `St4(d, x0, x1, x2, x3)` store helper (57w, arguments sunk), `__rlwimi(w >> 24, w', 8, 0, 23)` on all four rows (the intrinsic is
  scheduled differently and forces a frame: 143w) or on d[16] alone (57w), `Uint8 hi = w3 >> 24` / `(Uint8)(w3 >> 24)` (folded), a
  function-scope word set (range-split `@N` copies outrank the case locals: 148w), do/while, while, a hand 2-row body with its own `j`
  (84/57/97w), `-proc 750/603e/604e/7400/7450`, `-O3,p`/`-O4`/`-O4,s`, `-opt (no)schedule/(no)peephole`, `optimization_level 1..3`,
  `opt_common_subs/strength_reduction/lifetimes/dead_assignments/unroll_count/vectorize_loops` (57-345w). Two-definition pack
  VARIABLES (`x0 = w1 >> 24; x0 |= w0 << 8;` / `x0 = (w0 << 8) | x0;` / `x0 = __rlwimi(x0, w0, 8, 0, 23)`, declared `x0..x3` first so
  that descending-id colouring = statement order) would give the target's priority order, but the or->rlwimi merge then takes the
  `slwi` as its base (`slwi; rlwimi .., 8, 24, 31`, 87-110w) and the loads reorder -- the target's `srwi` base needs the one-expression
  `(w0 << 8) | (w1 >> 24)`. OPEN: what creates the pack temporaries of a row in reverse (or as frontend temps) while leaving the
  schedule and the stores as they are.
- **`#pragma opt_loop_invariants off` moves invariant hoisting from the frontend to a backend pass ("loop-code-motion" replaces
  "loop-transforms" in the dump), and the hoisted value is a BACKEND temp with the id of its position in the loop body.** With
  `p += (Uint32)stride & ~1;` in the case-2/6 loop and the pragma, the `rlwinm` is created at the end of the body (id 147, above the
  body's temps 135-146), hoisted to the preheader, and coloured before them: `clrrwi r3, r4, 1` = the target's register (ours as a
  variable or as the frontend's hoisted `@278`: r5, coloured after the temps). So the target's pitch has the priority of a LATE backend
  temp -- the original's frontend did not hoist that expression. Not applied (the temps' order above still costs the same 57 words and
  the pragma is function-wide); a level-2 explanation is excluded (pitch has 16 neighbours; r0 would be free for it).
- **Pins are not usable in these kernels**: every candidate register (r0, r3, r4, r5) is reused by other values of the same function
  (mc r3 in every case prologue, stride r4, the count r0), and a hard pin poisons the register function-wide (pass 18b).
- 8x8 4p (72w) reread: the divergence is the R1 tie at `addi p0+2` vs `add a2+b1` after `lbz b2` (both ready, ours picks the longer
  chain), not the registers -- the target's level-2 loop values a2/a3/a4/a5 sit in r31/r28/r29/r30 because the short-lived b0/b1/b2/a0/a1
  were coloured before them, which needs the target's tighter live ranges (ours: a2 32 / a3 47 / a4 42 / a5 33 / b5 30 / a6 50 / a7 35
  neighbours, all level 2 with r6-r11). `-proc` variants and `#pragma peephole off` do not give the target's order. The stride r0
  exclusion (target `li r0, 8` count first, stride r4 with `dcbt r6, r4` = stride in rB) is still unexplained: stride is a level-2 local
  above s0/s1/d (declaration order gives the rest of the prologue), and only an rA use (addi/D-form/dcbt rA) excludes r0.
- 16x16 H2: the target's average IS the mask-variable association (`add sh, m2; add wa, ·`, case 0 at 0xc4-0xcc), as in V2; with
  `MPVMC16_AVG2V` + `Uint32 m1, m2` locals the association matches but the case-0 schedule moves (the target issues `lwz w3` and the
  `w3 >> 24` shift before the first `rlwimi`; ours after) and `a3 = (w3 << 8) | w4` keeps the target's uncoalesced `mr r8, r20` copy
  (R3) where ours coalesces: 225 -> 300w, not applied.
- Harness hazard: `python3 try.py ... | grep` inside `for v in a b` loops -- zsh parses `== $v` as a command; quote the echo string.

**Residues (exact class):** mpv_mcy 1p 57w = the ascending temp order above (cases 1/5, 3/7: 9 words per unrolled copy; 2/6: 10 per
copy incl. `clrrwi r3`); mpv_mc 4p 72w (R1 tie + stride r0 exclusion), V2 73w (cases 1-3 `@N` copy order, pass 2), H2 436w (un-split
loop webs, pass 2); mpv_mcy 4p 136w / H2 225w / V2 225w (16x16 register pressure: R3 uncoalesced copies + level order of the
callee-saved set), untouched.

### CRI sfd_mpv/mwsfdcre pass 2

**Result.** `lib/sfd_mpv` 35/38 → 37/38 (`sfmpv_ExecServerSub` 17w → IDENTICAL, `sfmpv_ChkBufSiz` 181w → 18w, `sfmpv_DecodePicAtr` 197w unchanged). `lib/mwsfdcre` 7/10 unchanged (`mwPlyCreateSofdec` 97w, `mwsfcre_CreateSfd` 297w). No flags flipped; `ninja -k 0` + `dtk shasum -c` = 111 OK after the sfd_mpv edits. All edits are pure C, no `// COMPILER-DIFF:` tags.

**Cost model confirmed.** A python reimplementation of the 2.4.7 Chaitin allocator (vids ordered params → own locals reverse-declared → inlined-helper locals at their inlining point → frontend `@N` temps → backend expression temps in IR order; simplification scans vids upward removing degree < 29; when stuck remove min `Σ LOOPWEIGHT*(2*uses+defs) / current_degree`, ties → highest vid; colour lowest free of r0,r3..r12 plus callee-saved already handed out, else new callee-saved from r31 down) reproduces the compiler's colouring order exactly for ExecServerSub, ChkBufSiz, DecodePicAtr (including the SPILL picks) and for the pass-2 rerun of CreateSfd. Use it to test a hypothesis before touching the source: `ra.py` dump → interference graph → predicted colour list; if the prediction does not move the target register, the source change will not either.

**ExecServerSub (17 → 0).** Target colours the `MPV hn`/`SFTIM tim`/`bufout`/`bufin` values in registers that in ours went to the caller's own locals. Mechanism: locals of an inlined static helper are created at the inlining point, so they colour *before* the locals of helpers inlined later and before the caller's frontend temps. Moving the `stat == 2` user-header block into `sfmpv_DecUsrHdr(sfd, &ck, &used)` and the prep-flag tail into `sfmpv_ChkPrep(sfd)` (both `static`, defined after the caller with prototypes in the prototype block; `-inline auto,deferred` still inlines them) puts those values in the right vid range. Inside `sfmpv_IsEnoughData` the `Sint32 n` had to be declared *first* (before `mpv`/`hn`) so it colours r27 after `hn` r29.

**ChkBufSiz (181 → 18).** Two levers: (1) level-2 declaration order — `mpv` first, then `tabuf, fsize, ywidth, cwidth, ysize, csize, nfrm, i, n, h16, fsize2, tot, acc, n2, rfbuf, width, height, w16, frm` (the values that must take r31..r25 declared last); (2) the two frame-init loops as `static` helpers `sfmpv_InitRfbFrm(mpv, frm)` / `sfmpv_InitTaFrm(mpv, frm, n)` with `for (i = 0; i < n; i++, frm++)` — the `frm++` step as a source statement gives the target's `addi rX,rX,K` IV that pass-16 peephole-forward sinks to the end of the latch block. Residue 18w: target `w16` r8 / `height` addze temp r9 / `add sign,w16` temp r8 (needs r0 blocked at that point); ours swaps them. Tried: `register`, block scopes, expression permutations of the geometry statements, a `CalcWidth` helper, `w16 /= 2` variants — 18..32w. Constraint analysis of the target colouring leaves only that one temp (r82 in ours) unexplained.

**DecodePicAtr (197, unchanged).** Target callee-saved r17..r31 (15) vs ours r16..r31 (16). Target map: `mask` r26, `pts.lo` r25, `wk` r24, `prate` r23, `tmpref` r22, `d` one variable (init -1 in r22/r20, join copy `mr r22,r21`), `pts.hi` r19, `result` r18, `newgop` r17. Ours range-splits `d` (@1766 → r25/r26, coloured before `wk`) and spill-removes `mask` early (score 0.116, right after `newgop` 0.083, `pts.hi` 0.1, `pts.lo` 0.108). The target needs `wk/prate/tmpref/d` freed before `pts.lo`/`mask` are removed and `mask` coloured directly after `hn` — i.e. fewer neighbours for `d` (no range split) or a higher `mask` score. Not found in the time box; candidate: restructure the `d` init/join so the frontend does not split it, and give `mask` a loop-weighted use.

**CreateSfd (297, unchanged).** Both target and ours spill `width` in pass 1 and rerun (`regalloc-gpr-pass-2-*`); in pass 2 the top level is a single scan level coloured in vid order. Section pool bases (`...data.0`, `...rodata.0`, `...bss.0`) are created at function start as `mr r117..r119` + `lis/addi r120..r125` and the creation order is fixed data < rodata < bss regardless of TU declaration order, first reference in code, dummy referencing functions at TU top, or moving the `.data` definitions below `MWSFD_Malloc` — the target's `rodata r31 / bss r30 / data r29` therefore comes from pass-2 dynamics (rodata pool not scan-removable at its turn, degree ≥ 29, so it colours first), not from vid order. Ours removes the whole top group in one scan (rodata degree 13 there). The data pool is dead in both. Lever to look for: something that raises the rodata pool's pass-2 degree (more simultaneously-live values across the `trsetup` const references) or lowers bss's loop-weighted use count.

**CreateSofdec (97, unchanged).** Target frame 0x50 with `stmw r23` (`npool` r23, `vfreq` r25, `nfrm` r23, `sfdhn` r24) vs ours smaller frame; 12 extra local bytes in the target. Unused `Sint64`/`Float64`/array locals and a `Float64 ftime` produce byte-identical code to ours — the frame growth is not from a dead local. No lever found.

**Tooling notes.** Parallel `ra.py` runs (yours and another agent's) collide and produce no dumps — run sequentially. `-pragma 'cats off'` in the `ninja -t commands` line needs `eval` when replayed from a script. Harness `~/.cache/cri_mpv2/` deleted.

### CRI pass 21: the same-body local rank, the pcm redefinition, indexed body writes, and a validated Chaitin simulator (sfd_tim Matching 38 -> 39/39; adx_sje 12 -> 14/17, calc_rsig 41 -> 0, set_rsig 85 -> 0, encode_data 84 -> 68w; mps_lib .bss 0x10 OK; sfd_tst 79w / mpv_umc 68w read, not closed; 2026-09-11)

**Fixed (pure C, no pins):**
- sfd_tim `SFTIM_IsGetFrmTime` 6 -> 0 (unit IDENTICAL, flipped). The target's `tunit` ranks between the inlined
  body's `tscale` and `vrate` (r10 between r9/r11). Three passes tried wrapper locals, typed/`void *` copies,
  argument casts and declaration orders: every one is created either before all body locals (a wrapper's local,
  the call's argument temporary) or is propagated away. Only a local OF THE SAME inlined body can sit between
  two of its locals, so the function now inlines a frame-taking copy `sftim_IsGetFrmTimeFrm(sfd, frm)` with
  `Sint32 tunit;` declared between `tscale` and `vrate`, `tunit = frm->inf.raw[4]` read BEFORE `ftime =
  frm->inf.raw[3]`, and `if (sfd->cond[14]) return TRUE;` at the top (Tunit keeps the value-taking helper).
  Probes: tunit declared first or last = 11w/6w, ftime read first = 39w, a thin wrapper that loads tunit into
  its own local and calls the value-taking body = 6w (the copy is propagated). The two bodies are a duplicate
  by design; the original evidently had two.
- adx_sje `adxsje_calc_rsig` 41 -> 0: `pcm = sje->pcm[ch]` re-derived before the second loop. The redefinition
  is a range-split copy of the same address that coalesces away; its presence schedules the preheader's
  `li i, 0` above the pool `addi` and lets the post-RA peephole fold the 0x4330 constant's `lfd` onto its `lis`
  (the `addi rX, rY, @l; lfd f, 0(rX)` fold needs rX == rY, i.e. the constant's address in a dying register).
- adx_sje `adxsje_set_rsig` 85 -> 0: the nibble packer writes the output through an index (`n = -1; ... if
  (i % nibs == 0) { nib = 1; dst[++n] = 0; } byte = dst[n]; dst[n] = byte | (...)`), not a pointer. The
  strength-reduced pointer is then initialised `addi p, dst, -1` off the dying `dst` (a `dst - 1` in the source
  is constant-folded into the struct offset instead). The shift `(nibs - nib) * sje->bps` is written inline —
  an own local `sft` ranks below the tail temporaries and shifts the whole tail by one register.
- adx_sje `adxsje_encode_data` 84 -> 68: `void *obj` + `ADXSJE sje = obj` declared after `sjo` (kept copy,
  sje r29 = target). Left: `sji` r28 / `n` r27 (target r24 after cnt r25 / r20 after the loop-2 IVs) — the
  target colours `sji` between cnt's split copy (@544) and the loop-2 IV temps, and `n` between the loop-2 and
  loop-3 IV temps, i.e. both must be frontend temporaries created in those windows; helper-local/`void *`
  parameter/cast-argument forms of `sji` (67-92w) do not create them.
- mps_lib `.bss` 0x10/0x10: an unreferenced `static Sint32 mpslib_init_cnt;` after the referenced statics
  (MWCC emits unreferenced file-scope statics, after the referenced ones; an `= 0` initialised one would move
  before them). `MPS_Create` 2w stays (post-RA tie, pass 14b).

**Tooling: `tools/research/mwccdbg/rasim.py DIR [--drop vid..]` (next to ra.py/rasum.py) — a Chaitin simulator over
`regalloc-gpr-pass-1-all.txt` that reproduces MWCC's colouring order exactly** (checked on encode_data,
SFTST_Calc, OneReadMb): nodes whose flags say `fCoalesced` (not `fCoalescedInto`) are ghosts — never removed,
always counted in their neighbours' degrees; each iteration scans the remaining nodes by ascending vid and
removes every node whose current degree (physical neighbours + remaining virtual neighbours + ghosts) is
< 29, decrementing its neighbours as it goes (so a lower-vid node removed earlier in the SAME scan already
lowers a higher-vid node's degree); nodes removed in a later iteration are coloured first, and within an
iteration in descending vid. The `previous neighbors: N` field of the assigned dump is the degree at removal.
With it the level questions become arithmetic:
- mpv_umc `mpvumc_OneReadMb` 68w: `vx` (30 at its scan), `vy` (32), `cvx` (31) are exactly at the edge; the
  target has them in level 1. Removing 2 neighbours from vx (then vy needs 3, cvx 1 — vx/vy are scanned first
  and are cvx's neighbours) drops all three. Their extra neighbours are the ofs/tbl temporaries r67-r94 that are
  live while the vectors are; a source shape that finishes the vectors' uses (`>> 1`, `& 1`, `/ 2`) before
  the table/offset temporaries are created is what is needed. Not found this pass.
- sfd_tst `SFTST_Calc` 79w, two clusters. (1) `diff` lo/hi r23/r25 vs r25/r23: pre-RA the sub writes the
  backend pair r226/r227 and the ternary's else arm copies `mr @119.lo, r226; mr @119.hi, r227`; r227 dies in
  the diamond and coalesces INTO @119.hi (vid 96 survives), r226 lives to the hist store and cannot; so lo
  (vid 226) is coloured before the merged hi (vid 96) and takes r23. The target's pair = the merged hi coloured
  first (r23) then lo (r25, r24 = est.hi blocked) — either the survivor is the higher vid or `diff` is a kept
  variable (own vids 46/47, coloured after the IV temps: hi r23, lo r25). 25 more forms (`diff -= est`, no-op
  `(Sint64)`/`(Uint64)` casts on the assignment or the store, `0 - diff`, `>= 0` ternary, `adiff = diff = ..`,
  abs helper, `diff = diff` after) all leave the propagated backend pair. (2) the sprintf argument group
  (mt.cnt/out.cnt/mt_max/hlp.cnt loads and the MulDiv result) is shifted by one register: ours colours it
  r21-r30 from the bottom with mt.cnt.hi (@172.hi, 27 neighbours = level 1) last in r30; the target's group
  avoids r21 (mt.cnt.hi gets it last) and r23 for the MulDiv result — a node in r21/r23 interferes with the
  group in the target and not in ours. Not found.
- adx_sje `adxsje_write_end_code` 2w / `adxsje_output_header` 2w: post-RA ties (pass 14b), unchanged.

**Flags.** `"lib/sfd_tim.c": True` (CRI pass 21 block in objects.py). 111 OK. adx_sje 14/17, mps_lib 6/7,
sfd_tst 10/11, mpv_umc 15/16 stay False; sfd_tst and mpv_umc sources unchanged.

### CRI SWAR kernels pass 4: the 1p pack temporaries are @temps (helper-local per call gives the target's order), the first-word load is the open node; nothing applied (mpv_mcy 1p stays 57w; 2026-09-11)
Harness ~/.cache/cri_swar4/ (deleted): pass-3 `try.py lib/unit file.c [Func..] [--sbs Func] [--all]` (unit flags, strip_unused,
bytecmp with OBJ=, side-by-side `dtk elf disasm`), tools/research/mwccdbg `ra.py`/`rasum.py` (dumps ra_base, ra_vN1, ra_vN3, ra_vN5, ra_vN21,
ra_vK8). Task: test "static inline helper / block-local per pack step" for `MPVMC16_OneRef1p_TuneC` (lib/mpv_mcy, 57w), then the 8x8
kernels (lib/mpv_mc, not opened this pass: no shape reached the target for 16x16). `MPVMC08_OneRef1p_TuneC` asm untouched. No unit flipped,
no source change, 111 OK unchanged.

**Read off the dumps (verified, each with one `backend-10` + `regalloc-gpr` dump):**
- **Copies into OWN locals are never coalesced; copies into @temps are.** The or->rlwimi merge always emits `rlwinm t, lo, 24..; mr x, t;
  rlwimi x, hi, 8, 0, 23`. With x a function local the `mr` is a real node ranked by its declaration (N1: x0 r54 and its srwi temp r158 both
  in the priority list, `mr r5, r0` in the code, frame for b: 242w); with x an inlined helper's local or a CSE @temp the srwi temp is a
  ghost merged into x (K8 r159/r161/r163, N5 r155/r156/r157) and the code is the target's `srwi x; rlwimi x`. So the target's pack values
  are @temps (or backend temps) and its `lbz r12; mr r0, r12` means `b`/`h1` ARE own locals.
- **Helper locals are @N-numbered in REVERSE declaration order per inlining and in CALL order across inlinings.** N3 (each odd case as a
  `static void mpvmc16_cpN(MPVMC *mc, Uint8 *s)`): cp2's 13 locals are @280..@292 with the LAST declared (h1) at @280 = coloured first
  (h1 r3, w2 r4, w1 r5, w0 r6, p r7, d r8, i r9, x2 r0, x1 r10, x0 r11, pitch r12: 160w) -- the mirror of own locals (first declared =
  coloured first). N5 (one `static void mpvmc16_st8(Uint32 *d, Uint32 hi, Uint32 lo) { Uint32 x; x = hi << 8; x |= lo >> 24; *d = x; }`
  call per pack): the three x are @284/@286/@288 in call order and colour d[0]'s first: `srwi r3; srwi r4; srwi r5` = the target's
  ASCENDING order (pass 3's open question) with the `b` copy `mr r0` as in the target. Two-def `x = hi << 8; x |= lo >> 24` gives the srwi
  base (the `or`'s SECOND operand is the base, the first is fused) -- but the first def `rlwinm x, hi` stays as a dead instruction until RA
  and its output dependence delays the rlwimi (schedule `srwi; lwz w0; srwi; lbz; rlwimi T2; rlwimi T1`), and with the packs at r3-r5
  the loop values shift by one register and b spills to r31: 271w. `x = lo >> 24; x |= hi << 8` makes the slwi the base (pass 3).
- **The inliner gives an argument a @temp only when the argument has a side effect**; pure arguments are substituted into the body (N19
  `st4(d, P0, P1, P2, P3)` = base 57w; N21 with only `*(volatile Uint8 *)(p + 15)` in P3: P3 alone becomes @270 (b coalesced into it,
  r5), P0-P2 stay backend temps, 99w; N18 with nested word loads in every P: all four are @temps in the target's order, T1 r3/T2 r4/T3
  r5). **Arguments are evaluated RIGHT to LEFT** (N18's P3 `lbz` first; a nested-load chain across arguments is therefore UB and read
  the previous row's w3). N2 (`st(&d[k], P)` with the nested loads inside each call): the loads stay behind the inlined stores (148w).
- **The single-use first-word load (`w0 = *(Uint32 *)(p - 1)`, `h0 = *(Uint16 *)p`) is inlined into its use regardless of
  `#pragma opt_propagation off`** (Pb 62w, P5 175w, P19 62w: the 1/5 loop unchanged) -- it is not the frontend's propagation pass. It
  stays a variable only behind a store, an `if`, or `__dcbt` -- and `__dcbt` is also a scheduling barrier (N6: `lwz w0` stuck above the
  dcbt, W0 r3, 108w). Not blocked by: `(Uint32)*(Sint32 *)`, a `Sint32 w0`, a `Sint32 hi` parameter, `*(volatile Uint32 *)` (a
  `*(volatile Uint16 *)` IS kept: ETYPCON of a volatile load, N7 `lhz r3`), a word pointer `q = (Uint32 *)(p - 1); w0 = q[0]`, inlined
  `ldw(p + k)` helpers for the other words (N12-14), dead nested pointer assignments `*(Uint32 *)(q = p - 1)` (q removed first; the
  pass-14 anchor needs the nested variable live). A nested `(h0 = load)` inside another def anchors the OUTER def and is itself
  substituted when single-use (N1/N3 `lhz r0` backend temp).
- **Parse-time folds** (no temp, no code; K9/K10/K14/K16/K17 = base 57w): `v | v`, `v & v`, `(v ^ v) ^ v`, `if (v != v)`, `(v << 0) | v`,
  `(0, e)`. Kept: `v ? v : v` (172w), a self-assignment `(w1 = w1) >> 24` (not removed; makes the slwi the base, 98w).
- **Target model of the 1p loop (1/5):** pack temporaries = @temps coloured in statement order (r0, r3, r4), `b` an own local with the
  uncoalesced copy (r0), the first-word load an own local coloured AFTER the first two packs (r4), the loop values own locals in
  declaration order (stride r5 .. b r12); 2/6 the same with pitch a late backend temp (pass 3, `clrrwi r3`) coloured before the packs
  and `h0` after them. Every form here that makes the packs @temps (call-order helper locals, side-effect arguments) leaves the first-word
  load a backend temp coloured first (r0, packs shift to r3-r5, +1 register, frame): OPEN = a codeless way to keep `w0`/`h0` a variable
  (or a @temp created after the second pack) with pack @temps that carry no dead first def.

**Residues (exact class, forms tried this pass):** mpv_mcy 1p 57w (above); mpv_mcy 4p 136w / H2 225w / V2 225w and mpv_mc 4p 72w / V2
73w / H2 436w untouched (the 1p shape was the precondition). N1 242w, N2 148w, N3 160w, N5 271w (= Na/Ne/Nh/Ni/N12-17 conversions,
pointers, load helpers, dead anchors), N6 108w, N7 171w, N18 107w (UB), N20 98w, N21 99w, K15 172w, Pb/P19 62w, P5 175w.

### CRI pass 20: breadth-first inlining numbers the ids, user copies are never coalesced, index loops put the IV copies in the preheader (sfd_cre AnalyAudio 43 -> 0, AnalyMps 28 -> 0, AnalyMpv 15w open; sfx_zmv MakeCnvZTbl 12 -> 0, MakeOrgZ32TblByCCIR 74w open (post-RA schedule); adx_tsvr 2w / sfh_main 6w unchanged; nothing flipped; pure C, no new tags; 2026-09-11)

Units read: lib/adx_tsvr (5/6, `adxt_nlp_trap_entry` 2w), lib/sfh_main (35/36, `SFH_AnlyElemSmpHz` 6w), lib/sfd_cre (3/6 ->
5/6), lib/sfx_zmv (6/8 -> 7/8). Judge `tools/bytecmp.py`; dumps with mwccdbg `ra.py` (frontend ASTs, backend passes,
regalloc pass-1 all/assigned).

**Mechanisms (each confirmed on the dumps, then reproduced in the real source):**
- **Inlining is breadth-first (FIFO).** The depth-1 callees of a function are cloned in call order, then each clone's
  callees in order. Ids (pass 11 ranking) follow the clone order in DESCENDING creation order: a depth-1 helper's locals
  outrank every depth-2 temporary, and a depth-2 helper called after another depth-1 helper's callees is processed after
  them. In sfcre_AnalyMps: `sfcre_AnalyPackSiz` (depth 1) -> its three `sfcre_SearchDelim` clones and then
  `sfcre_MpsMuxRate` (depth 2); `sfcre_AnalySfdHdr` (depth 1, the header search loop) -> `sfcre_SetSfdHdrInf`. Moving the
  loop into a depth-1 helper is what puts its `i r27, ps r28, p r29, n r30` above the pack-start searches' temporaries,
  and moving `mps` into a depth-2 helper called after the searches gives `mps r22` after p3.
- **Within an inlined helper** later-declared locals get the higher id (coloured first); parameter clones rank above the
  helper's locals; a call result assigned to a helper local propagates into the `@ret` temp (the local vanishes), an
  arithmetic-defined local survives.
- **The RA coalesces only compiler copies** (`@ret`, argument moves, `?:` copies). A user copy `res = cur` (macro form
  of a loop) always leaves its `mr`; the `bf 2, next; b found` found-path shape is a deleted coalesced `@ret` copy.
- **Expression CSE temps rank below inline temps.** `size - (p1 - data)` written twice as an expression gives frontend
  CSE temporaries created last (just above own locals), so the search pointer p1 keeps r30 with `ofs r29, n r28`; the same
  values as locals would rank above it.
- **MWCC does not inline a function containing a label/goto**, and auto-inline has a size limit: the search loop plus the
  header copy plus the four field stores is over it (`static inline` does not help; `#pragma inline_max_size /
  inline_max_total_size(100000)` inlines everything and wrecks the unit, 718w). Splitting the field stores into a second
  helper (`sfcre_SetSfdHdrInf`) brings the loop helper under the limit.
- **Loop layout:** `for (;;) { if (hit) break; advance; if (i >= 3) return; if (n <= 0) return; i++; }` gives the target's
  block order; `while (!hit) { ... }` rotates the loop (26w).
- **Strength-reduced index loops initialise their pointer IVs in the loop preheader** (after the `cmpwi 0, 256; bf`
  guard) where user copies `src = orgtbl; dst = tbl;` sit in the guard block and are hoisted around the compare. sfx_zmv
  MakeZ32Tbl/MakeZ16Tbl linear loops as `tbl[i] = orgtbl[i] & 0x7FFFFF80; tbl[i] <<= 1;` / `tbl[i] = (Uint16)(orgtbl[i]
  >> 15)`: 12 -> 2w; the Z32 perspective loop `*dst = ...; src++; dst++` (src incremented before dst): 2 -> 0w.
  Declaration initialisers (either order, `i` first or last, mixed) all keep the copies in the guard block.

**Open, with the exact class:**
- sfd_cre `sfcre_AnalyMpv` 15w: the target computes `ofs + 1` into a fresh r0 and colours the header bytes b4 r4, b7 r5,
  ofs r6, b6 r7, b5 r6 (b5 reuses ofs's register); ours folds `ofs + 1` in place and numbers the bytes differently.
  Declaration/statement orders of the byte loads and `ofs` (about 30 forms over passes 8-20) do not move it.
- sfx_zmv `sfxzmv_MakeOrgZ32TblByCCIR` 74w: **ours' pre-RA schedule (backend pass 17) is instruction-for-instruction the
  target's final order** of the 8x-unrolled 1.164f loop body (same registers, same 452 instructions); ours' post-RA list
  scheduler (pass 23) re-hoists the constant hi-word `stw r0, 8(r1)` and the `addi i-k` chain. The target's block was not
  re-scheduled after RA and nothing in the bytes shows why (block B8 is 91 instructions at pass 22; the 70-instruction
  `tbl[i] = ztbl[ytbl[i]]` block matches). Not the mechanism: a `k = i - 16` / `Float32 f` local, a `k++` counter (235w),
  a `q = ytbl + 16` pointer (113w), `(Uint8)` cast, operand order, a `static` luma-table helper (99w), a hand-unrolled
  `i += 8` body with eight `k0..k7` locals (133w, smaller .text: the compiler's unroll is the target's).
- adx_tsvr `adxt_nlp_trap_entry` 2w and sfh_main `SFH_AnlyElemSmpHz` 6w (M4 stwbrx fold / size 0x15c vs 0x14c): read
  again, no zero-code form; the pragma forms (`peephole off`, `scheduling off` around the callers) do not close them.

**Applied (pure C, no new tags):** src/lib/sfd_cre.c (`sfcre_SetSfdHdrInf`, loop helper `sfcre_AnalySfdHdr(p, n, inf)`,
`sfcre_MpsMuxRate`, expression-form `sfcre_AnalyPackSiz`, flat `sfcre_AnalyMps`; AnalyAudio's `sfcre_MinLe` /
`sfcre_SkipPketHd` helpers and two-web `n`), src/lib/sfx_zmv.c (index-form linear loops, `src++; dst++` order). Neither
unit flips (AnalyMpv 15w, CCIR 74w).

### CRI pass 22: the two-step address (addi-into-addi is never re-propagated), the 8-word array-register limit, and a run-once chain of nine sfd_buf mechanisms (sfd_buf Matching 21 -> 26/26, flipped; sfd_mps 22/26 with .rodata OK and DecodeOneUnit 316 -> 144w at target size; cftfx / adx_sje unchanged; 2026-09-11)
Harness ~/.cache/cri22/ (deleted): `bld.sh UNIT SRC OUT.o` (the unit's MWCC GC/2.7 flags + strip_unused, no ninja),
`try.sh UNIT SRC [FUNC]` (bytecmp with `OBJ=`), `tryvar.py UNIT BASE.c VARS.py [FUNC] [--only ..] [--keep NAME]` (`V = {name:
[(old, new), ..]}` unique-substring edits, ~0.3 s per variant), `fd.py UNIT FUNC [OBJ] [--all]` (llvm-objdump side by side,
branch targets relative, relocation symbols folded in; tools/fdiff.py runs ninja unlocked -- do not use it while others build).
tools/research/mwccdbg/ra.py + rasum.py + chaitin.py for every ranking question; chaitin.py reproduced DecodeOneUnit's colouring
(`--check`) and was used as a library to test "what if" graphs before touching the source (see the model_dou notes below).

**Mechanisms read off the dumps (verified, pure C):**
- **Add-propagation never propagates an addi it has itself just rewritten.** `wk = &hn->w` (addi 0x1308) and `ring = &wk->u.ring`
  (addi 0x10): the pass folds `addi wk` into wk's loads AND into ring's addi (addi-into-addi -> `addi ring, hn, 0x1318`), but that
  rewritten addi is not propagated into ring's own loads/stores, so `ring` stays a node (a callee-saved register across the calls,
  `hn` dying in r3) -- sfd_buf's OPEN `ring`/`sup` class since pass 12b, mpv_cmc's `oi` since pass 15. Conditions: the intermediate
  pointer must be an OWN local with at least two uses (a single-use `wk` is propagated by the frontend and the constants folded into one
  addi, which IS propagated; a helper parameter is the same), and the base must not be reassociated: `&sfd->buf[n]` becomes `sfd +
  (n*0x74 + 0x1308)` (mulli; addi 4872; add) -- keep the SFBUF_HN view `hn = (Uint8 *)sfd + n*0x74` with its own uses (sj, used loads
  through hn) and `wk = &hn->w` as the second step. Applied in DestroySj (three `wk = &sfd->buf[k]; sup = &wk->u.ring.sup; if
  (wk->mode ..)` blocks: three addi's, one register), SetSupplySj (`ring` computed before the mode test, declared before `hn` so it
  colours r29 above hn r28), RingAddRead/AddWrite. The mechanism is the same class as InitHn's `addi 3, 31, 56` being folded: there the
  ring addi is an original addi off the parameter `wk`.
- **The backend's array-register transform registerises a stack array only up to 8 words (32 bytes)**: `Uint32 adr[9]` stays in the
  frame (target InitHn frame 160 = adr[9..11], the 16-byte rounding hides the exact count), `adr[8]` becomes 8 registers (stmw r21 vs
  r27). It also needs the loads to be plain: with a running value (`a = prm->adr; for (i < 7) { *p++ = a; a += prm->size[i]; } *p = a`)
  the chain is one register (`add 7, 7, rSize` in place, stores interleaved), the first InitRing reloads adr[0] from the frame
  (`lwz 3, 92(1)`) and the size loads are software-pipelined one add ahead; `adr[i + 1] = adr[i] + size[i]` gives seven separate
  temporaries with the stores sunk to the end and no reload. `p++` in the for-header vs in the body changes the schedule (L3 vs M6/M7).
- **A helper that reads a `Sint32 *size` twice** (InitVfrm/InitAout): `used = (*size != 0)` computed into a local BEFORE the
  `wk->mode` store, and `wk->u.vfrm.size = *size` after the adr store -> the target's early `lwz size` + a reload after the store
  (a by-value `size` never reloads; `wk->used = (*size != 0)` after the mode store cannot hoist the load above the store).
- **The unroller's guard threshold is 8 iterations**: a 7-store clear has no guard (`for (i < 7) rsv[i] = 0`), a 10- or 12-store loop
  gets `cmpwi; bf` + 8x body; the target's 10 aout words = the rsv[7] loop + 3 explicit stores (aout `rsv2[3]` added to sfd.h,
  nobody else reads aout), its 12 uoch words = `for (i < 3)` over the four SFUO_CH fields.
- **The first arm's `ret = 0` (13b/14b `bne body; b end`) in a big body: the whole body as a `static inline` helper** whose locals are
  declared in REVERSE of the target's colouring (helper locals: first declared = lowest vid = coloured last), the second `sj` a
  separate `sj2` (a frontend range-split copy is created last and takes the lowest free handed-out register, r25, instead of r26), and
  `ring->dlm_pos` read inline in the four compares (a `pos` helper local outranks the frontend's ck.data CSE temporaries and takes r3;
  the inline reads make pos a CSE temporary of the same generation). A plain-function `return ret;`/`return 0;` in the arm becomes
  `li r3, 0` (the frontend knows ret == 0 there but still keeps the redundant store), `asm {}`/`(void)`/dead stores invert the IF.
- **Named .rodata objects are emitted in DEFINITION order** (sfd_mps: the `SFD_tr_sd_mps` interface table defined before
  `sfmps_CopyPketFn` with a prototype block, .rodata OK); literals still follow @N creation order (12b).
- **`*nskip = *nbyte = delim = 0` chain assignment** shares one zero (target `stw r24` from delim's `li r24, 0`): three statements
  (any order) give a separate `li r0, 0` because the backend CSE only merges temporaries with temporaries; the frontend constant-
  propagates `*nbyte = delim` back to a literal.
- DecodeOneUnit's structure (all read off the target): a single `return ret` at the end of an if/else-if chain (endcode skip 1,
  endcode skip 2, `delim == 0`, `!(flags & PKET)`, packet copy) -- five `return ret;` statements give five `mr r3, ret` copies;
  `delim != END ? go = FALSE : (IsEndcodeSkip || IsSystemEndcodeSkip ? go = FALSE : go = TRUE)` for the three `li` sites; the zero-
  byte scan as `static Bool sfmps_IsZero(Uint8 *data, Sint32 n)` (its own pointer copy `mr r4, data`, `li 1` after the loop = the
  helper's `return TRUE`; ours advanced the shared `p` past the nonzero byte -- a real bug); `total < 4 && sfmps_IsInTerm(sfd)`
  (helper local `term` = the lowest frame slot 8, `term2` an own local declared last = slot 12); frame order syshd, flags, hdrlen,
  copied, cres, term2. 316 -> 144w at the target size; the residue is the level structure: target L3 = {ret, wk, nskip, nbyte, len,
  data, sfd} (7 new callee-saved r31..r25 in that vid order), ours L3 = the four params only; chaitin.py says ret needs +3 neighbours,
  or TWO extra live-everywhere nodes (ghosts) move ret/wk/data up together (`all+2` in the model = 11/13 registers with the declaration
  order p, delim, ret, wk, mps, bufin). Two kept copies live across the function is the class; not found (13 forms).

**Not closed:** sfd_mps ExecServerSub 59w / CopyPrvate 60w (target has a `ret` variable r24 and a kept `len` copy `mr r5, r24`, frame
+16) / CopyPketData 157w untouched; cftfx UserTable 135w (four row pointers y0..y3 with `y1 = y0 + (ywidth - 4)` collapse further,
0x204; the target keeps three COPIES of `ywidth - 4` (`addi r31, r11, -4; mr r7, r31; mr r4, r31`) each consumed in place by one
`add rX, yPrev, rX` and recomputes the fourth -- a frontend range-split of the hoisted CSE temp, spelling not found), StaticV 60w,
Argb420 38w and the size-4 `...rodata.0` pool untouched; adx_sje encode_data 68w: block-scoped `sji`/`n` inside the do body, re-read
per iteration, or `sje->sji` as the argument (113w) do not create the frontend temporaries the target colours between cnt's split
copy and the loop-2/loop-3 IV temps -- block scope changes nothing for the ranking (own locals either way); write_end_code /
output_header 2w post-RA ties untouched.

**Flags.** `"lib/sfd_buf.c": True` (CRI pass 22 block in objects.py), 111 OK (the concurrent `game/motion.o` failure in `ninja -k 0`
is another agent's half-written unit). sfd_mps, cftfx, adx_sje stay False; sfd.h gained `aout.rsv2[3]` (union size unchanged).

### CRI sfd_mpv/mwsfdcre pass 3 (sfd_mpv 37 -> 38/38 IDENTICAL, unit Matching, pure C, no pins: ChkBufSiz 18 -> 0, DecodePicAtr 197 -> 0; mwsfdcre 7 -> 8/10: CreateSofdec 97 -> 0, CalcWorkSfd 6 -> 4w, CreateSfd 297 unchanged; chaitin.py saved under tools/research/mwccdbg; 111 OK; 2026-09-11)

**Tooling (PERSISTENT).** `tools/research/mwccdbg/chaitin.py OUTDIR [--pass N] [--check] [--verbose] [--nb]` = the
python model of the 2.4.7 GPR Chaitin allocator (pass-2 cost model, re-implemented), fed from an `ra.py` dump directory;
`--check` diffs it against the compiler's `regalloc-gpr-pass-N-assigned.txt` (order, colour, degree at removal, cost) and
is IDENTICAL on sfmpv_ExecServerSub, sfmpv_ChkBufSiz, sfmpv_DecodePicAtr (9 spill picks), mwPlyCreateSofdec, mwsfcre_CreateSfd
pass 1 (incl. the `width` spill) and pass 2, plus the four dumps of this pass. Usage note in that README. Details that the
pass-2 paragraph did not have, all verified against the compiler's own `cost:` lines: (1) `fCoalesced` nodes were merged INTO
their `->` target (a physical register for argument moves, another vreg for copies) and stay in every neighbour list as never-
removed ghosts; `fCoalescedInto` leaders take the union of their members' neighbours minus the members; (2) cost = sum over
the class's instructions, intra-class `mr` copies excluded, of LOOPWEIGHT*(2*uses+defs); the base of an update-form
load/store is a use and a def; a node whose SINGLE def is `li`/`lis` costs sum(w_use) - w_def (rematerialisable: r141 `li 0`
with 5 uses = 4, a multi-def constant variable is charged normally); (3) the parameter moves and the backend-inserted copies
have no line number in the PCode; (4) the colouring picks the lowest-numbered free register of r0,r3..r12 and the callee-saved
registers already handed out (ascending), else a new one from r31 down; (5) --pass 2 has no PCode of its own, the costs are
the compiler's carried-over `cost:` lines. As a library: `g = chaitin.load(DIR); chaitin.allocate(g)` after editing
`g.adj/g.cost/g.order` answers "does one more neighbour / a lower vid move the target register" before touching the source.
Harness ~/.cache/cri_mpv3/ (deleted): `bld.sh UNIT [SRC]` (unit's exact flags into a scratch object, `OBJ=`
bytecmp), `fd.py UNIT FUNC` (dtk disasm side-by-side, no ninja), `tryvar.py UNIT variants.py FUNC [--fd] [--keep L]`,
`apply.py variants.py LABEL file` (the winning replacement list applied to the tree source). ra.py runs kept sequential.

**Read off the dumps (verified by the builds):**
- ChkBufSiz 18 -> 0: `csize = (h16 / 2) * (cwidth = (w16 / 2 + 31) / 32 * 32);` -- the nested assignment creates the
  h16/2 sign chain (r87..r89) BEFORE the w16/2 chain, so h16/2 (r0, level 2) and ywidth (r7) are live across the cwidth
  temporaries: r83 loses r7 (ywidth) and r0 (h16/2) and takes r8 = w16's register, the rest of the chain in place. Both
  operand orders of the product work; `ysize = h16 * ywidth` moved before it.
- DecodePicAtr 197 -> 0 (all the mechanisms, each one confirmed by a variant):
  * `d`: the running time computed in a block-scoped `Sint64 dd` inside `if (ent.pts >= 0)` with `d = dd;` as the block's
    last statement. The frontend splits `dd` (subtraction) from its clamped web @N as before, the backend coalesces the LOW
    word of the block-end copy and keeps the HIGH word (`mr r22, r21`), so d keeps its -1 init in r20/r22 and the @-pair
    (r25/r26, coloured before wk/prate/tmpref) disappears -- the 16th callee-saved register with it. A function-scope `dd`
    keeps both copies (`mr r22, r20; mr r23, r21`); `t` as the temporary and `if (d < 0) d = 0` forms are worse.
  * the reform section as `static inline sfmpv_ReformTc(sfd, mpv, atr, d)` with helper locals `newgop` (reloaded there) and
    `reform` declared last: reform r17 > newgop r18 > ChkGopTc's `ttu1` r19 (as own locals of DecodePicAtr they colour
    ttu1 r17 / newgop r18 / reform r19, because an own local ranks below every @ temp; helper locals are @ temps numbered at
    the inlining point, last declared = highest vid, a nested inline's locals below the outer's).
  * the ttu1 block as `static inline sfmpv_ChkGopTc(sfd)` returning `flag`: flag's `li 0` is CSE'd with the zero of the
    caller's 64-bit compare (one `li r5, 0`, pass-14b helper-local rule; as an own local it never is), and the target's
    `bne body; b test` pair is `if (ttu1->valid == 0) { flag = 0; } else { ... }` -- the emptied THEN arm leaves the pair, an
    emptied ELSE arm folds to `beq`. The helper's t1/t2/unit keep the slots 8/c/10 (declared in that order).
  * `d < 0` (was `d > 0`): the target's `subfc r0, r5, r20` operand order = `d < 0` (force the reform when a PTS exists).
  * `SFSEE_VRAW *raw = (SFSEE_VRAW *)vhdr->raw; raw->len = n; MEM_Copy(raw->dat, ck->data, raw->len)` = the target's
    `stw/lwz 0x200(r3)` off the raw pointer (pass-15 class: an address value not folded because it is the call argument).
  * `swk = sfd->see.wk; wk = SFMPV_WK(sfd);` before the test chain: both loads land before the `bne` (target), swk r4 / the
    mpv reload r3 (swk = later-declared own local, wk's second def a range-split @ coloured before it).
  * `len = ck->len` as a local declared AFTER `n` (n r0, len r4: the two-use load is a variable coloured after n, where the
    load temporary outranks n and takes r0); `d = -1` before `pts = -1` (init order); `ttu3, ttu1b` declared in that order
    but assigned `ttu1b = ...; ttu3 = ...;` (declaration order gives the colours ttu3 r18 / ttu1b r17, statement order the
    two addi -- initialised declarations are emitted in REVERSE declaration order); `inf->picrate` stored before
    `inf->bitrate`, `vb = vbvsiz; br = bitrate;` (the target loads vb first).
- CreateSofdec 97 -> 0: the frame difference (0x50 vs 0x40) is the 9th callee-saved register (stmw r23), no extra local:
  MWCC's frame = 8 + locals + 4*nregs rounded to 16. The target colours the inlined mwSfdDestroy loop temporaries (ptr, i,
  addr of the 7 FreeAll copies) r23/r24/r25 = the lowest handed-out registers, i.e. AFTER npool/vfreq/sfdhn and the
  ATTACH block's buf/usize/nskip; ours coloured those own locals last. Fix: `mwsfcre_AttachPicUsrBuf(mwply)` (locals pu,
  buf, usize, nskip) and `mwsfcre_SetSfdCond(mwply, lw)` (ftime, nfrm, npool, vfreq, sfdhn) as `static inline` helpers --
  round-1 helper locals outrank the round-2 (nested FreeAll) strength-reduction temps @409/@410/@480. Colouring order then:
  nskip r27, usize r25, buf r24, sfdhn r24, vfreq r25, npool r23, nfrm r23, loop temps r23/r24/r25, zero copy r26. The
  bps r0 / mode r4 swap = CWS_BUFSIZ (block-local `mode`, the CalcWorkSfd copy) instead of MWSFCRE_CALC_BUFSIZ with the
  function-scope `mode` (only CreateSfd needs `mode` afterwards).
- CalcWorkSfd 6 -> 4: `Sint32 height; Sint32 width; width = ...; height = ...;` in MWSFCRE_CALC_FRMSIZ (colours follow
  the declaration order, the two loads the statement order). Open (4w): the target's last add is `add r3, sib, size` with
  the epilogue `lwz r0` not hoisted; `size = sibsiz + size; return size;` (and `return (size = ...)`, a `size2`, a helper,
  `(Sint32)(Uint32)` casts) is substituted into the return and the 0x4800 addi is folded past the add (7-15w);
  `sibsiz += size; return sibsiz` bounces through `mr r3` (9w). The epilogue is merged into the last body block by the
  peephole (single predecessor) and the final scheduler then hoists `lwz r0` above `addi; add` but not above `mr r3, r29`.

**CreateSfd (297w, open) -- what the model says.** In ours' pass 2 the top level is one scan of 17 nodes (mwply first at
alive-degree 28, ..., data 14, rodata 13, bss 12): everything removable, coloured in vid order bss r31 / rodata r30 / data
r29 / cwk1.. temps r28..r23 / mode r22 / nfrm r21 / height r20 / frmret r19 / adxibuf_p r18 / adxwk_p r17 / cprm r16 /
mwply r15. The pool creation order is fixed data < rodata < bss (first uses in the initial PCode: bss line 41, rodata 142,
data 1430 -- NOT first-reference order), so rodata can only beat bss if it is NOT removable at its scan turn while bss is:
exactly 16 lower-vid top nodes still alive when rodata is scanned (12 physical + bss + 16 = 29) and bss then at 28. With
the current 17-node top group that needs all of them to survive their own scan turn, which the model shows is impossible
(they are at 28 or less; one more node makes them all stuck and the picks go by cost/degree, again bss > rodata > data). So
the target's pass-2 top group is a different SET: its registers say cwk1 r28, cwk2 r27, frmret r26, adxibuf_p r25, adxwk_p
r24, picusr/hnwork r23/r22, mode r21, (width2/frmtbl ptr) r20, nfrm2 r19, nfrm r18, height r17, cprm r16, mwply r15 -- frmret
/adxibuf_p/adxwk_p/nfrm2 above mode/nfrm/height (own locals in ours: they are @ temps or helper locals in the target, e.g. the
adxt/frame allocation blocks as inlined helpers), and the unit's `mwsfd_mps_trsetup` table reads are what gives the rodata
base its 23 uses. Not attempted in the time box; the next agent should dump the variant and run chaitin.py --pass 2 --check
before every build.

**Flags.** `lib/sfd_mpv.c` True (bytecmp IDENTICAL, 38/38). `lib/mwsfdcre.c` stays False (8/10). `flock ... ninja -k 0` +
`dtk shasum -c` = 111 OK. Note: another agent edited the same sfd_mpv functions concurrently during this pass and converged
on the same ChkGopTc helper (its comment is the longer one in the file).

### CRI SWAR kernels pass 5: mpv_mcy 16x16 1p 57 -> 0w (pure C, the in-place sliding window); the frontend's substitution and web rules read off the dumps; nothing flipped (mpv_mcy 3/5 functions identical; 2026-09-11)
Harness ~/.cache/cri_swar5/ (deleted): `try.py lib/unit file.c [Func..] [--sbs Func] [--all]` (unit flags compile of a scratch
copy under src/lib, strip_unused, bytecmp with OBJ=, side-by-side `dtk elf disasm`), tools/research/mwccdbg `ra.py`/`rasum.py` (dumps of ~15
probe TUs: base, v3a/v3b, e2, f1, k1, r1, r2, y1, z1, mcv2a). Units: lib/mpv_mcy (1p 57 -> 0, 4p 136, H2 225, V2 225 untouched),
lib/mpv_mc (4p 72, V2 73, H2 436 untouched; 1p asm untouched). 111 OK before and after; objects.py untouched (mpv_mcy not identical).

**Applied (pure C, zero code): `MPVMC16_OneRef1p_TuneC` cases 1/5, 3/7, 2/6 rewritten as the in-place sliding window**: the words
are loaded in ADDRESS order (`w0 = *(Uint32 *)(p - 1); w1 = ..(p + 3); w2; w3; b = p[15];`), the source pointer is stepped BEFORE the
packs (`p += stride;` — 2/6: `p += (Uint32)stride & ~1;` with no `pitch` local), each word is redefined with its packed value
(`w0 = (w0 << 8) | (w1 >> 24); w1 = (w1 << 8) | (w2 >> 24); w2 = (w2 << 8) | (w3 >> 24); w3 = (w3 << 8) | b;`), then the four stores
`d[0] = w0; d[1] = w1; d[16] = w2; d[17] = w3;`. Function byte-identical (schedule, registers, the `mr r0, r12` copy of `b`).

**Why it works (each fact read off a frontend/backend dump, single-variable probes):**
- **The frontend's single-use substitution has a fourth blocker: a statement between the def and the use that REDEFINES a variable
  the RHS reads — including the pointer of a load.** `w0 = *(Uint32 *)(p - 1); ...; p += stride; ...; d[0] = f(w0)` keeps `w0` a
  variable (AST y1; the code is unchanged because an own local coloured after the base's four backend temps also takes r3), while
  without the step it is substituted regardless of `register`, `const`, `(Uint32)*(volatile Sint32 *)`, block-scope declaration,
  `*(Uint32 *)&p[-1]`, a union view, a second pointer variable, an empty `asm { }`, or `#pragma opt_common_subs / opt_dead_assignments
  / opt_dead_code / opt_loop_invariants / opt_strength_reduction / opt_propagation off`. Pass 4's list (store, if, dcbt) + this.
- **A range-split web is sunk into its single use only if none of its RHS operands is redefined before the use.** In-place packs
  `w0 = (w0 << 8) | (w1 >> 24); w1 = (w1 << 8) | (w2 >> 24); ...`: w0's pack web reads w1, which is redefined next, so it survives
  as an @temp coalesced with its `srwi` (no `mr`: copies from a backend temp into an @temp coalesce; into an OWN local they never do —
  v3a `srwi r0; mr r12, r0`, r1 x2/x3); w3's pack (`(w3 << 8) | b`, nothing redefined before `d[17] = w3`) is sunk = the backend temp
  behind the target's `lbz r12; mr r0, r12` (b is an own local kept by the three stores). Without redefinition, a reused variable's
  LAST TWO webs are sunk and the earlier ones survive (g2/v3a/f1/k1: 3/4/5/6 defs), the first web being the own local.
- **@N numbering = the order of the variables' FIRST definition in the function; within one variable the later web gets the LOWER
  @N.** z1 (loads w1, w2, w0, w3): @367 = w1's pack, @368 = w2's, @369 = w0's, @370 = w3's -> coloured T1 r0, T2 r3, T0 r4, w0 r3;
  loads in address order (z2): T0, T1, T2 = r0, r3, r4 and the own locals stride r5, i r6, d r7, p r8, w0 r4 (lowest free: T2 is
  not a neighbour), w1 r9, w2 r10, w3 r11, b r12 = the target. f1 (one `t` reused): web 2 @368, web 3 @367 (backwards) — so a
  single reused temporary colours in reverse statement order, four distinct words colour in load order.
- **A frontend-hoisted invariant is an @temp numbered with the early hoists (@27x, next to the dcbt's stride copies), i.e. coloured
  right after the backend temps and before the pack webs**: 2/6's `p += (Uint32)stride & ~1;` gives `clrrwi r3` (target); a `pitch`
  own local is coloured after the pack webs (r5). Replaces pass 3's "late backend temp" reading; the 2/6 colouring is then T3 (mr,
  backend) r0, T0 web r0, pitch r3, T1 r4, T2 r5, own locals i r6 .. h1 r12, h0 r4.
- **The or->rlwimi merge (peephole-forward) fuses the `rlwinm` operand DEFINED EARLIER in the raw order and keeps the later one as
  the base** (base: `rlwinm r151 (slwi); rlwinm r152 (srwi); or r153, r151, r152` -> `mr r153, r152; rlwimi r153, w0`; v3b: `t = w1 >>
  24` defined first -> t fused, slwi base). Operands are evaluated LEFT to right (pass 1's "A | B evaluates B first" is wrong). So a
  variable holding the srwi (`x = lo >> 24; x = (hi << 8) | x` / `x |= hi << 8`, own local or helper local) is always fused: the
  two-def helper forms (pass 4 N5, s3 here) are dead ends — N5's dead `rlwinm x, hi` is removed after RA but its output dependence
  moves the second `rlwimi` before the first. `__rlwinm(lo, 8, 24, 31)` as x's def is fused too (s1); `__rlwinm(w0, 8, 0, 23) | (w1
  >> 24)` keeps the intrinsic as base (t6) — intrinsic operands schedule differently (loads pulled up), unusable here.
- **Helper locals are @N-numbered in reverse declaration order per inlining (r2: `x3, x2, x1, x0` -> @277, @276, @275), CSE temps
  after them in first-occurrence order (@293-@295) and created as NESTED assignments at the first occurrence** — which anchors the
  enclosing def (r2's `x1 = ((@295 = LD) << 8) | ..` survived with one store between; r1: an own-local x1 with the same anchor
  survived too — its `mr` vanished only by register luck). CSE'd loads colour before the function's own locals (w1 r8 .. above
  p/d/i/stride r9-r12), so the words of these kernels are own locals of the function, never helper locals or CSE temps.
- Single-def helper locals (`Uint32 x = P; *d = x;`, `ret` locals, macro block locals) and every codeless second use tried
  (`x = x`, `(void)x`, `if (x != x)`, `x++` dead, unused return value, volatile store, `do {} while (0)`, block, `x = *d`, `hi = x`,
  `y = x`, `x = 0`, pre-loop `w0 = 0`) are removed before the propagation decision: webs need LIVE defs.

**Residues (exact class):** mpv_mcy 4p 136w / H2 225w / V2 225w — the averaged kernels keep every word live for the AVG, so the
in-place form does not transfer (V2 case 1 with address-order loads + `s0/s1 += stride` before the packs: 249w); mpv_mc 4p 72w / H2
436w untouched; mpv_mc V2 73w: the target's `lbz r31 (a2); mr r28, r31; rlwimi r28, r30` with `w2` in place — `s1 += stride` between
a2's load and its use keeps a2 a variable in the AST (mcv2a) but the backend coalesces the dying own local into the pack (`lbz r29;
rlwimi r29`, 80w): a copy FROM an own local coalesces, so the target's copy needs a2 live past the `or` or a pack destination that is
an own local (first web of a variable used only in cases 1-3) — not resolved.

### CRI pass 23 (paused by the user; adx_tsvr 2w / sfh_main 6w / mps_lib 2w unchanged in the tree; mpv_umc OneReadMb 68 -> 56w found in the harness, NOT applied; nothing flipped; 2026-09-11)
Harness ~/.cache/cri23/ (deleted): tryv.py/tryumc.py substring variants over tools/research/kit/variant.sh, ra.py dumps. No tree file was edited.
- **mpv_umc `mpvumc_OneReadMb` 68 -> 56w (harness only, verified with variant.sh, not applied):** (1) `mc->src2 = ypitch + yhx + src;` for the LUMA
  block (B4 identical: the add chain rule is `a + b + c` -> `t = b + c; r = a + t`, so the target's `add r0, yhx, src; add r0, ypitch, r0` needs ypitch
  first; chroma keeps `src + cpitch + chx`), 68 -> 60; (2) `cpos = ofs[0] + (cvx >> 1) + (cvy >> 1) * cpitch; ypos = ofs[1] + (vx >> 1) + (vy >> 1) *
  ypitch;` (target `add r29, cvx>>1, mul`), and (3) `vx = mv->vec[0]; vy = mv->vec[1]; ypos = ...;` placed BEFORE `cvx = vx / 2` (the target's
  `srawi vx>>1`/`srawi vy>>1` are the 3rd/6th instructions after the loads = statement order; `lwz ofs[1]` before `lwz ofs[0]`), with `yhx = (Uint32)vx & 1;`
  after `fn_y = ...` and `chx = (Uint32)cvx & 1;` after `fn_c = ...` (the cast keeps the frontend from CSE-ing the table index): 60 -> 56w. Left (56):
  vx r6 / vy r23 / cvx r25 / cvy r24 (target r25/r11/r28/r7): vx is still coloured before the temporaries and vy/cvy after all of them; the ra.py
  dump of that variant was being read when paused. Variant text: harness u4.py 'a2' (== a1/a5/a6/a7 at 56w; cpos before fn_y 62-67w).
- **mps_lib `MPS_Create` 2w:** backend-16 (before the post-RA scheduler) already has the target's `li r4, -1; addi r0, r3, @l` order; the post-RA
  pass swaps them. Not moved by: 9 statement orders, `-1` in a local, a `volatile MPS_OBJ *mps` (all 2w), `#pragma scheduling off` (72w), an
  `asm { li r4, -1; mr m1, r4 }` pin (9w: `lis r3` cannot cross the asm and the Sint64 fields need `srawi` hi words), an asm lis/addi of the fn address (17w).
- **adx_tsvr `adxt_nlp_trap_entry` 2w:** the pass-19b 3-pin form reproduced (2w moved to `subi r5, r4, 1` before `mr r3, sji`); the asm `lis` has no
  latency, so its C consumer is ready one cycle early and outranks the argument move (target: lis latency 1, subi at cycle 2). Not fixed by: `lim = hi - 1`
  before/after `ofst2 = 0`, the asm at the function top, an `asm { mr r3, sji }` copy (4w), lis+subi inside the asm with r5 pinned (10w), `scheduling off`
  (108w); zero-code forms (`ofst + ofst1`, temps, `-= -ofst`, volatile view) leave `lha r0`.
- **sfh_main `SFH_AnlyElemSmpHz` 6w (M4):** backend-14 (before the post-RA peephole) is the target's final code to the instruction except the word
  register (r4 vs r6); the post-RA peephole then folds to `stwbrx`. It folds even when the swapped value stays live (`return s | 1`: chain AND stwbrx
  emitted), with asm-defined rlwinm partials, dead asm copies, volatile stores, statement-split ors (7-8w) and 16-bit halves (4w, a different chain).
  `#pragma peephole off` for the function: 10w (also disables the pre-RA rlwinm/or merge: the C SWAP32 then gives rlwinm x4 + or x3, 14w); a private
  peephole-off GetElem copy with `hdr[0x198 + i*0x40]` indexing: 38-41w. Left at 6w.

### CRI pass 24 (stopped by the user before any build; sfd_mps 22/26, cftfx 3/6, adx_sje 14/17 unchanged; no source or objects.py edit; 2026-09-11)
Only sfd_mps DecodeOneUnit (144w) was analysed, with ra.py + chaitin.py as a library (harness ~/.cache/cri24 deleted). Read off the
model against the target's registers (ret r31, wk r30, nskip r29, nbyte r28, len r27, data r26, sfd r25, delim r24, mps r23, total r22,
bufin/dst/scan-counter r21, p r24), all UNVERIFIED by a build:
- The colouring rule "lowest free among the callee-saved registers already handed out, sorted ASCENDING" explains bufin/dst/n = r21 in
  both: once r21 is handed (bufin), every later L1 node takes r21 before r23/r26. So the target's scan counter is coloured AFTER bufin,
  i.e. it is an L1 node (degree <= 28 at its round-1 scan turn; ours @671 sits at exactly 29 = L2) with a vid below bufin's: an OWN local
  distinct from the syshd `n` (`cnt`), plus the hn-block Bool as a separate own local declared after it (ours reuses `go`, whose
  range-split @669 has a higher vid and is still alive at the counter's turn). p then colours r24 after r79 (delim's 2nd web) without
  any data edge (I first misread this as p-data interference).
- With @671 leaving the round-1 survivors, ret needs +3 permanent neighbours (ours 27 at its round-2 turn), data +1, wk +1: only
  ghosts (call results copied into a variable: `mr rG, r3; mr var, rG`, rG coalesced into r3) or vreg->vreg coalesced @ret copies
  qualify. Candidates: the two SFSET_GetCond OBJ results as a kept `void *obj` local (target `mr r22, r3 .. mr r5, r22` = the variable,
  the coalesced @ret its ghost; +2), and one more not identified (a helper @ret copied into a local, e.g. shdr/GetSeeShdr).
- IsZero: target `mr r4, data; mtctr psize(r3); lbz; addi p,1; extsb.` = psize as a frontend CSE @temp (`sfd->prm.unit` written twice
  inline, coloured r3 before the helper's pointer copy r4) and a `Sint8 *p; if (*p++ != 0)` body (`extsb.`, increment before the test).
Variants file (obj / psz / isz / cnt / ok combinations) was written but never compiled. cftfx and adx_sje not started.

### CRI pass 25 (sfd_adxt 26/28, cri_cvfs 11/13, sfd_tst 10/11 unchanged; stopped by the user after the AdjustSync read; no source or flag edits; 2026-09-11)

- sfd_adxt `sfadxt_AdjustSync` 51w: chaitin.py `--check` is IDENTICAL on our dump (ra.py out, level 2 = @329 wk r31, skipbyte,
  endflg, skip, nch, params). Target colours read off the listing: level 2 = wk r31, skip r30, skipbyte r29, nch r28, endflg r27
  (endflg, not astart, must be the r27 before nbyte r26: astart does not touch skip/skipbyte), params r26..r23; level 1: lim r22,
  frmbyte r21, sfreq r29, astart r27, vstart r22, tim r21, p(@317) r30, ofs(@318) r28, n r21. **The target's sfreq r29 needs an
  r30-coloured neighbour, and the only r30 nodes are skip/p, so the target's `skip` interferes with sfreq (and astart/vstart/tim):
  its `li skip,0` sat above the SetStartTime argument moves pre-RA.** In ours `li r43,0` is scheduled one slot below
  `subf vstart-astart` (skip touches none of them). Putting `skip = 0;` before `SFTIM_SetStartTime(tim, vstart, sfreq);` emits the
  `li` BEFORE the `bl` (53w, the call is a block boundary the scheduler does not cross) — not the target's shape.
- Random+hill-climb search over the 12 own-local declaration orders on our graph (with/without skip edges) tops out at 15/18 target
  colours; the graph itself differs (tim 28 and sfreq 28 neighbours sit exactly at the level edge: with the skip edge both would
  jump to level 2, so the target's tim/sfreq have >= 1 neighbour fewer — the @ret bounce ghosts r63/r68 or the GetAudioInf temps).
  Also free: the frame wants `dmy` declared before `vflg` (target vflg 0xc / dmy 0x10, ours swapped) — not applied.
- cri_cvfs cvFsGetFileSize 45w / cvFsOpen 152w and sfd_tst SFTST_Calc 79w: not started this pass (bytecmp .rodata/.bss `pad` on
  sfd_tst/cri_cvfs is accepted by bytecmp: the verdict is `IDENTICAL (pad: ...)`, no static needed).

### CRI mwsfdcre pass 4 (stopped early by the user; no source edits; CreateSfd 297w, CalcWorkSfd 4w unchanged; 2026-09-11)

Pool-base ranking mechanism of `mwsfcre_CreateSfd` READ OFF PROBES (ra.py, verified 3 ways, not yet applied):
- The per-section pool bases (`...data.0`/`...rodata.0`/`...bss.0`) are backend temps created at function entry in the
  REVERSE of the order in which the body first references each section (probe4: body refs bss, string, data -> created
  data, rodata, bss = ours; probe5: refs string/rodata table, data, bss -> created bss, data, rodata). Later-created =
  higher vid = coloured first, so the target's rodata r31 / bss r30 / data r29 means its body references a .rodata
  object (a string, with `-str readonly`; the unit's only rodata uses in this function are the error strings) BEFORE
  the first .bss reference, and .data last. Ours references bss first (the CALC_BUFSIZ stores to sib/vib/..).
- The order is taken from the caller's FINAL (post-inline) body, not the parse order: a static helper holding the bss
  stores, called first, still puts bss first (probe6 = probe4 order). So the lever is a real rodata reference (an
  error-string call, e.g. a prm check `if (cprm == NULL) MWSFSVM_Error(...)`, or the CALC_FRMSIZ block with its
  E206011 string) placed before the CALC_BUFSIZ stores in the target's source, NOT a helper boundary.
- Pooling itself needs enough objects per section (2 statics do not pool, 9 do; a deferred `ftab` alone does not).
- The pass-3 "different top set" reading is unnecessary: with rodata's vid above bss's the existing 18-node top group
  (rfbret spill pick, then 17 in vid order) already gives r31 rodata / r30 bss / r29 data. Still open in the same
  group: frmret must rank between cwk2 and picusr (a helper local / @temp created between the two inlined Mallocs),
  and mode > adxibuf_p > adxwk_p > nfrm > height (own-local declaration order or helper locals). chaitin.py --pass 2
  --check is IDENTICAL on the current dump (r229 = rfbret is the L4 spill pick -> r14, as in the target's `mr r14, r3`).
- mwPlyCalcWorkSfd 4w not touched. Harness ~/.cache/cri_mws4 (probes 1-6, ra dumps, fdiff captures) deleted.

### CRI SWAR kernels pass 6: 16x16 4p target read (mpv_mcy 4p 136w, H2 225w, V2 225w; mpv_mc 4p 72w, V2 73w, H2 436w — all unchanged; paused before any edit; 2026-09-11)
Harness ~/.cache/cri_swar6/ (deleted). No source or config edit; 111 not re-run (nothing changed). Read off the `MPVMC16_OneRef4p_TuneC`
target and ours' dumps (ra.py, 4p base) in the first 20 minutes:
- Ours already has the target's sum association `p_k = a_k + (((a_{k+1} + b_k) + b_{k+1}) + 2)` and the pack chain (rlwinm p1 base,
  rlwimi p0, p2, p3): the frontend keeps `p7 = ...` as an own variable (its def is the statement right before the `d[0]` store) and
  substitutes p0..p6 into the two store statements; AST holds `p7 = (b8 + (b7 + (a7 + a8))) + 2`. The residue is schedule + RA only.
- Target facts: (1) the loads of pixel pair 9 (`lbz 9(r5)/9(r6)`) are scheduled BEFORE `stw d[0]` and pair 10 after `stw d[1]`; ours
  never moves a load across a store (checked on ours' a9..a16), so the target's raw order loads pixels 0..9 before the first store.
  (2) `add a16+b15 .. add a15+..` (d[17]'s p15) sit before `stw 0x40(r3)`: the target's body has NO block split between d[16] and
  d[17]; ours splits at statement 71 (backend-00 B3 = 112 instructions, the >100 rule of pass 16b) so d[17] cannot interleave.
  (3) 11 callee-saved (`stmw r21`) vs ours 7 — longer live ranges from the wider schedule. (4) Colours: stride r0 (before the ctr
  `li r7,0x10` temp), d r3, i r4, s0 r5, s1 r6; 8x8 target is the reverse for stride/ctr (ctr r0, stride r4).
- 8x8 4p target (72w) schedule is near statement order (loads in address order, a8 before a7, `p0.3` before `p1.1`); the 16x16 one
  is not height-sorted (b5 loaded 11th, before b3/a4/a0): the scheduler looks windowed over the raw order, not a pure critical-path
  list scheduler — the raw (source) order of the loads/sums is the lever. Next step: shapes that load 10 pairs before `d[0]`
  (or compute p8's inner sum there) and keep the body under the 100-instruction split, then chaitin.py for the colours.

### CRI pass 26 (adx_dcd5 ADX_DecodeMono4 39w read off the dumps, nothing changed; adx_baif not started; stopped by the user; 2026-09-11)
Harness ~/.cache/cri26/ (deleted). No source/config edit; adx_baif 170w / adx_dcd5 39+118+180w unchanged.
- **Mono4's 39w are one graph fact, validated with chaitin.py (`--check` IDENTICAL on the base dump):** target L2 colouring
  order is `@174 smul-ext r0, @176 c2 r8, @177 c1 r7, i r10, l2 r11, l1 r12, sc r31, sadd r30`; ours is `sc-temp r64 r0,
  @174 r10, .., l1 r11, l2 r12, i r31, sadd r30`. Replaying the model with the ids `i > l2 > l1 > sc > sadd` (sc scanned
  between i and sadd, i.e. an OWN-LOCAL id, r41..r46) reproduces the target's eight L2 colours exactly. So the original
  declared the counter before the histories (`i` highest, then `l2`, then `l1`) and its scale value was an own-local
  node, not the backend temp.
- Why ours loses the node: `sc = (Sint16)(X)` is `ETYPCON long(ETYPCON short(EADD))` -> backend `extsh r64,r63; mr sc,r64`
  and copy propagation replaces the single-def `sc` by r64 (backend temp id = top of L2 -> r0). A single conversion
  (`d = src[0]` = `lbz; extsb d,d`) is emitted straight into the variable. Tried and rejected (variant.sh, 61w each):
  `Sint16 sc` (holds the unextended value; the frontend hoists `(long)sc` as an @temp r53 and re-extends), reusing `s`
  for the scale (`s = (Sint16)(..)` is range-split into @temp r49 above the own locals, then propagated the same way).
  Both also re-hoist `lis AdxQtbl` above the extshs (l1 -> r31 new callee-saved), so the declaration order `i, l2, l1`
  alone is not enough: the scale node must be fixed first. Open: a spelling whose AST has ONE conversion at the top
  of `sc = ..` (e.g. a short-typed intermediate that the frontend does not substitute), or a 2-def own local whose
  copy is coalesced rather than kept as `mr` (the target has no `mr`).
- Ste4AsSte/AsMono (118/180w, 0x2f0/0x2ec): the same class (c1/c2 in place, one fewer callee-saved) — not dumped this pass.

### CRI mwsfdcre pass 5 (CreateSfd 297 -> 126w, pure C, no pins: the pool-base order is a TU-wide rule, frmret/picusr..fname via inlined helpers, own-local order, CALC_BUFSIZ arm order, mpvpara block; CalcWorkSfd 4w unchanged; 8/10, not flipped; 2026-09-12)
Harness ~/.cache/cri_mws5/ (deleted at the end): `pools.sh SRC [FUNC..]` (unit flags, no strip, per-function pool
`lis/addi` prologue), `gen.py PERM..` (one probe function per PERM, `b`/`r`/`d` = 6 refs of .bss/.rodata-strings/.data in that body
order; runs ra.py and prints the creation order from backend-00), ra.py/chaitin.py dumps under `ra_*`.
- **Pool-base creation order = the REVERSE of a TU-WIDE list, built in the order in which the sections were first POOLED by the
  functions compiled so far (definition order), a new section appended when the current function first references it (body
  first-reference order within that function).** Verified with 6 single-function permutations (creation = exact reverse of the body
  order) and 6 two-function TUs (`f0` refs d,b then `f1` refs b,r,d -> f1 creates r,b,d; `f0` r,d,b -> f1 b,d,r; `f0` with ONE bss/
  string/data reference (not pooled) registers nothing). Pass 4's "reverse first-reference order" holds only for the first pooling
  function of the TU. Later-created = higher vid = coloured first (r31). Pooling of a section needs several references in the
  function (1 ref: direct `lis/addi`; 6 refs: pooled; threshold not measured).
- **mwsfdcre: ours had `mwPlySetFrmBuf` (dead, keeps the .bss first-reference order) as the FIRST function -> it pooled .bss first ->
  L = [bss, rodata (MallocCompoWork), data (CreateSfd)] -> CreateSfd bases data r29 / rodata r30 / bss r31. The target's rodata r31 /
  bss r30 / data r29 = L [rodata, bss, data]: no function before mwsfcre_MallocCompoWork pooled .bss.** Zero-code fix applied:
  mwPlySetFrmBuf moved after mwsfcre_MallocCompoWork (it is stripped, so .text is unchanged; .bss layout unchanged because it is
  still the first function to reference bufnum/bufsize/bufptr/adxibuf/..). CreateSfd 297 -> 247w, 8/10 unchanged. **.bss is laid
  out in TU first-reference order** (verified: without mwPlySetFrmBuf the layout becomes sib, vib, aib, adxibuf, adxwk, sjb, bufnum,
  rfb, tab, bufsize, bufptr, sisjadr = CreateSfd's body order).
- Target register map of CreateSfd (read off the listing, corrects pass 3's guess): L5 group rodata r31, bss r30, data r29, cwk1 r28,
  cwk2 r27, frmret r26, picusr_p r25, hnwork_p r24, buf700_p r23, fname_p r22, mode r21, adxibuf_p r20, adxwk_p r19, nfrm r18,
  height r17, cprm r16, mwply r15, rfbret r14 (L4 spill pick); L2: block-1 width2 r20 / height2 r19, block-2 nfrm2 r19, width2 r22,
  height2 r20, fsize r22, frmtbl ptr r20, i r23.

### CRI pass 28 (cri_cvfs cvFsGetFileSize 45 -> 36w, cvFsOpen 152 (-4 bytes) -> 56w (sizes equal); sfd_adxt AdjustSync 51 -> 47w; sfd_tst SFTST_Calc 79w read, unchanged; no flip; 2026-09-12)
Harness ~/.cache/cri28/ (deleted): variant.sh copies, ra.py dumps, `exp.py`/`exp_gfs.py` (chaitin.py replays with edited `g.adj`/`g.order`).
- **cri_cvfs (structure fixed, ranking left):** the target's `addi tbl,r31,cvfs_tbl` sits AFTER the first search's `bl strlen`, is copied
  into search 1 and 2 (`mr dev, tbl`) and used in place by search 3. Reached by making the first search's prologue ResolveDev's OWN
  statements: `name = dev; if (name == NULL) name = cvfs_defdev; len = strlen(name); tbl = cvfs_tbl;` then the loop as a helper
  `cvfs_FindDev(tbl, name, len)` (`return cvfs_tbl[i].vtbl` keeps the target's `addi r3; lwzx`; `return tbl[i].vtbl` drops 8 bytes).
  Rejected: `dev = cvfs_tbl` inside the helpers / `cvFsGetDevIf(cvfs_tbl, dev)` as the argument (the parameter is propagated to its
  single use: the addi lands after the strlen but becomes `dev` itself and the second search recomputes it, 114w); `&cvfs_tbl[0]`,
  `(CVFS_DEV *)cvfs_tbl`, `cvfs_tbl + 0` do not CSE. The `sprintf(path, "%s:%s", ..)` passes `name` (r26), not `dev`. cvFsOpen's
  -4 bytes: `void *hn = cvfs_AllocObj(); obj = hn;` keeps the inlined @ret in r3 + `mr r30, r3` (the pass-12 typed-kept-copy rule);
  the Open call reads `obj->vtbl->Open` twice (target reloads obj->vtbl into r3 for the test and the call).
  Residue = register ranking, read with chaitin.py on the new dump: (a) target pdev r28 / tbl r27, ours reversed: both L2, tbl is the
  helper local @1384 (id 48) above the own local pdev (id 33, 28 neighbours at removal) — pdev needs one more L2 neighbour (or tbl a
  level lower). (b) the two SearchDev copies' `i` and the vtbl chain (ret2 -> ResolveDev vtbl -> caller vtbl, one coalesced node) are
  r26 in the target = `name`'s register reused, coloured BEFORE r25/r24/r23 are handed out to i1/dev1/len1; ours colours the first
  search's nodes first (ids r58-60 > r53-56 > r48-51: later call = lower id) so i2/i3/chain get r24. The target's first-search clone
  therefore has the LOWEST ids of the three (cloned last, e.g. FindDev called from inside another depth-1 static helper) — not tried.
  AllocObj's loop `i`/`p` r3/r4 swap unchanged.
- **sfd_adxt AdjustSync:** `dmy` declared before `vflg` = the target frame (vflg 0xc / dmy 0x10), 51 -> 47w. `skip = 0;` before the
  SetStartTime call still emits the `li` before the `bl` (49w with the frame fix, wrong shape). The remaining 47 are pass 25's fact
  (target skip interferes with sfreq/astart/vstart/tim; tim/sfreq one neighbour fewer). ExecServerSub 112w not started.
- **sfd_tst SFTST_Calc 79w, two priority facts (chaitin.py --check IDENTICAL on ours apart from 4 cost lines):**
  (a) abs region: ours colours diff.lo (backend temp r226, `mr diff, sub` propagated) at L2 before adiff.hi (@119 r96, coalesced with
  diff.hi) -> diff.lo r23 / diff.hi r25; target diff.hi r23, diff.lo r25, adiff.lo r22 = diff.lo ranked below @119 (the `diff`
  variable kept, id r46). `diff = ..; adiff = sftst_Abs(diff)` (static helper `if (v < 0) v = -v;`) and the plain two-statement form
  both move the `mr adiff.lo` above the branch (117w) — the ECONDASS-in-condition form is still the only one with the target's arms.
  (b) sprintf block: nine mutually interfering values (out.cnt @171 r60/61, mt.cnt @172 r58/59, mt_max @173, hlp.cnt @174, the
  MulDiv result r403) take r21..r30 in colouring order. Target order: mt.hi, out.hi, out.lo, MulDiv, mt.lo, mt_max.hi/lo, hlp.hi/lo;
  ours: MulDiv, out, mt.lo, mt_max, hlp, mt.hi last (27 total neighbours = L1 -> r30). With our ids no degree change reproduces it
  (r403 would need a level below @171 and above @172.lo; mt.hi a level above everything or an id above r403): the target's temp
  numbering differs (its MulDiv value ranks between out.cnt and mt.cnt, its mt.hi above out.cnt). Also ave.hi r22 / tol.hi r23 in
  the target = tol.hi (@123 r94, L2) coloured before ave.hi (r45, L3 in ours: 28 at removal after the L2 scan). Frontend note: the
  argument CSE temps are created in REVERSE argument order (@171 = out.cnt from args 9-11 first, @174 = hlp.cnt last) and
  `tst->hlp.cnt / tst->hlp.unit` (the __div2i operands) is not CSE'd with @174.

### CRI SWAR kernels pass 7: the >100 block split is exact and unavoidable from C for the 16x16 4p body; target reads for the others (mpv_mcy 4p 136w, H2 225w, V2 225w; mpv_mc 4p 72w, V2 73w, H2 436w — in progress; 2026-09-11)
Harness ~/.cache/cri_swar7/ (deleted at the end): `try.sh <unit> <variant.c> <FUNC> [--all]` (variant.sh + log), `ours.sh <log>` (the ours column
of the side-by-side), `gen.py NAME BODY [PRE]` (base.c with one function body replaced), `mkrow.py`/`mkrow3.py` (4p row-body generators),
`tasm.sh <unit> <FUNC>` (numbered target listing), probe1-8.c (split-rule probes), ra_* dumps.
- **Block-split rule, probed exactly (probe1-8, backend-00):** the backend starts a new basic block before a statement whenever the
  current block already holds > 100 PCode instructions (counted as EMITTED: `x = x*3+1` = 2, `x += p[k]` = 2, `(x<<22)&M | y` = 2,
  a 24-instruction nested statement = 24; 50 two-instruction statements = 100 -> no split, 51 -> split). It fires before assignments,
  pointer stores and `if` statements alike, inside loops (LOOPWEIGHT 8) and straight-line code, with `#pragma optimization_level 2/3`,
  `scheduling off`, `peephole off`, every `opt_* off` pragma, inside an inlined helper (the helper's own statements are the boundaries,
  `inline_max_size`/`inline_max_total_size(100000)` needed to inline a 130-instruction helper), inside a single-line macro body,
  inside `do { } while (0)`, and with comma-joined stores (`d[16] = X, d[17] = Y;` becomes two ST_EXPRESSIONs). The blocks stay
  separate through scheduling (B3 120 / B4 28 in backend-08), so nothing crosses the split.
- **Consequence for MPVMC16_OneRef4p_TuneC:** the target's loop body is ONE block of 124 final instructions (34 lbz, 64 add/addi, 16
  rlwinm/rlwimi, 4 stw, dcbt, 3 pointer steps, cmpwi, bne — 131+ initial with the 7-op packs) whose `cmpwi i,7` is scheduled among
  the d[17] pack (127 of 134) and whose p12..p15 sums interleave with the d[16] pack, so no split exists anywhere in it; under the
  rule above every straight-line C body of that size splits (ours before `d[17]`, B3 = 111). An inner 2- or 4-iteration loop unrolled
  by the backend would evade the check but cannot give the target's load set (each pixel loaded once, pair 9 before `stw d[0]`, pair
  10 after `stw d[1]`; a load never passes a store in either direction within a block — q1 probe). Left open: what disabled the check
  in the original (a flag/pragma of CRI's build, or a statement kind that is not a check point); not a C-shape question in our tree.
- Verified on the way: loads written as expressions inside the sums are NOT CSE'd across the `d[k]` stores (pixel 4/8 reloaded, h1
  0x25c); an inlined-helper row body puts the loop variables first in the colouring (d r3, s0 r5, s1 r6 as the target, b1 122w) because
  the pixel values become helper locals ranked below the own locals; `register`/block-scope/nested-assignment forms do not move the split.

**ToolEspArea's last 7 words (the ctor block's r28<->r29 local-alloc tie, `lis/addi esp_area_work` refs 4 life 48 vs `li 4` refs 2
life 12, both pri 1666), 35 min of header variants judged on all four includers with `variant.sh`; nothing applied:**
- Statement order `rows = nRows; numWork = n; pWork = work;` (and pWork anywhere later) -> t_esp_area 7 -> 2, BUT t_lightarea 4 -> 6,
  t_event 141 -> 143: the pair flips (work's life is 49 at RA: 1632 < 1666, the constant is allocated first and takes r29), yet the
  final store order becomes `stw numWork; stw pWork` in all three ctor blocks (the target has pWork first). The pWork/numWork store pair
  IS LUID-ordered at sched1 and sched2 keeps that order; the pass-"cDbgToolMain ctor boundary" claim that the store order is not
  LUID-driven only holds for the rows store (its constant is the one hoisted above strlen). So the target had pWork's store before
  numWork's at sched1 AND work living one insn longer at RA: an RA-time insn inside work's life that emits nothing.
- Codeless `asm("" : "=m"(top))` anywhere BEFORE the `w = strlen(name)` statement (first statement, after `x = wx`, after `y = wy`,
  with `"r"(this)`): the register pair flips (e08/e14/e1c identical) but the anchor takes the single free issue slot before `bl
  strlen` in which sched1 hoisted `li 5` (rows): rows becomes a post-strlen `li r10,5`, the pre-strlen stores reorder, and a global
  pair r17/r18 (`mr r18,r28; mr r17,r29` at +0x218) swaps -> 15 words; t_lightarea 13, t_event 143. The same anchor AFTER strlen
  (after `h = 1`, `pName = name`, `rows = nRows`, `pWork = work`) changes nothing: dying stores rank above a weight-0 asm at sched1, so
  it lands after `stw pWork`, outside work's life. Carrying the rows constant in the anchor (`"r"(nRows)`, with `"=m"(top)` or
  `"=m"(rows)`) -> 23 words (the constant's refs 3 re-rank it). `asm("" : "=m"(x))`/`"=m"(y)` as the first statement -> no change
  (dead, the store to the same slot follows).
- Register launders `asm("" : "+r"(v))` at the ctor top: wx 25, nRows 269 (the constant stops being hoisted anywhere), work 17 (and
  t_event 228), wy 23, n 30 -- all worse; `"=m"(pWork) : "r"(work)` after the stores 19 (refs 5 raises work's priority).
- Left: the anchor must occupy an issue slot before `bl strlen` WITHOUT displacing the `li 5` hoist -- i.e. an RA-time insn that
  sched1 places in the block's first group (the `lis/addi/li` argument setup before `bl __builtin_new`) and that has no memory
  operand and no register input that re-ranks anything. No such C/asm form found; the caller side (t_esp_area.cpp) cannot help
  (pins are impossible through the inlined actual; an anchor after the ctor makes the address a global allocno, 226 words).
  Flip order unchanged: t_esp_area needs IDENTICAL first, t_lightarea's 4 words are its vtable relocs into t_esp_area's copies.
### CRI pass 29 (sfd_mps 22/26: DecodeOneUnit 144 -> 13w with one M1 pin, ExecServerSub 59 -> 16w with one M3 pragma; cftfx 3/6, adx_sje 14/17 in progress; 2026-09-11/12)
Harness ~/.cache/cri29/ (mk.py unique-substring variant generator + run.sh over tools/research/kit/variant.sh, ra dumps, deg.py /
ghost2.py = chaitin.py as a library with injected ghost nodes). The 01:07 tree reset wiped the first application; re-applied from the record.
- **The residue of both sfd_mps functions is the same graph fact, validated with chaitin.py as a library: the target has 2-3 more
  "ghost" nodes (coalesced copies that stay in every neighbour list) than ours.** DecodeOneUnit: on the pure-C graph (cnt/ok own
  locals, IsZero `Sint8 *p; *p++`), injecting THREE ghosts adjacent to the whole-function nodes (any mix of positions, at most two of
  them while `mps` is still alive) reproduces the target's 14 colours exactly (ret r31, wk r30, nskip r29, nbyte r28, len r27, data r26,
  sfd r25, delim/p r24, mps r23, total r22, bufin/dst/cnt r21); two are not enough (ret stays 28 = L2). ExecServerSub: TWO ghosts
  adjacent to everything give sfd r31 / skiptot r30 / total r29 / tot r28 / data r27 / ret r26 / len r25. A ghost = a copy chain from
  a call result (`mr rT,r3; mr var,rT` -> rT coalesced into r3, `mr rT,r3; cmpi rT` is propagated instead and leaves nothing; a
  vreg->vreg coalesced copy such as an inlined helper's `ret = @local` also counts, r47->r43 in ExecServerSub). Not found: which
  three copies the original had in DecodeOneUnit (13 spellings of `obj`, `flg`, helper wrappers around the late SFCON/GetTermFlg tests
  all propagate away or add a visible `mr`).
- DecodeOneUnit facts (pure C, applied): the scan counter is an own local `cnt` distinct from the syshd `n`, the hn-block Bool a
  separate `ok`, both declared AFTER `psize` and BEFORE `hn`/`go` (cnt's round-1 degree is exactly 30: `ok` and `hn` must be removed
  before its turn, so they need lower vids = later declaration; with ret pinned to r31 cnt gains r31 and stays L2 -> r23 instead of
  r21, the 5 remaining scan words). IsZero as `static Bool sfmps_IsZero(Sint8 *p, Sint32 n)` with `*p++ != 0` and `sfd->prm.unit`
  written inline three times (`lbz; addi p,1; extsb.` = target); the target colours psize r3 / pointer r4 = the psize node outranks the
  inlined param copy — ours has the CSE temp @666 below the inline copy @646 (13 spellings incl. GetUnit helper, Uint32 view, `(Sint8 *)
  data`, swapped params: all 13w). The 120 declaration-order permutations of p/delim/ret/wk/mps do not move a word; `register` does
  nothing. Applied lever: `err = sfmps_CopyPketData(..); asm { mr r31, err; mr ret, r31 }` (M1) — pinning through `ret = 0` is
  constant-propagated away (`li r31,0` deleted, r31 merely blocked), pinning the SetErr def gives 124w, the CopyPketData def 13w.
- ExecServerSub facts: `li ret,0; mr skiptot,ret; mr tot,ret` = the pass-14b entry-zero CSE, which only rewrites @temps: the loop
  body + flow-count block live in an inlined `static Sint32 sfmps_ExecServerLoop(SFD sfd)` whose locals are declared
  `nskip,nbyte / rcnt,wcnt / r / limit / len / ret / data / tot / total / skiptot` (helper locals: first declared = lowest vid; frame
  wcnt 8 / rcnt 0xc needs `rcnt, wcnt`), with three separate `= 0` statements (a chain assignment is constant-propagated into three
  `li`). +1 ghost each from `ret = sfmps_Decode(..)` (a one-line wrapper around DecodeOneUnit) and `ret = sfmps_AddRead(sfd, nbyte)`
  (the RingAddRead + `ret = 0; if (r) ret = r` block as a helper; its `ret` local coalesces into the caller's ret) -> sfd L3 r31. The
  remaining 16w = ret r25 / len r26 swapped: the AddRead helper's local is the coalescing LEADER (lower vid than len); without it ret
  outranks len but sfd is one neighbour short. Side effect: the small ExecServerSub is inlined into SFMPS_ExecServer (0x1dc vs
  0x20) -> `#pragma dont_inline on/off` around SFMPS_ExecServer only (M3; `on` before ExecServerSub would also stop its own inlines).
- Wrappers that do NOT create a ghost (result propagated): GetCond, GetMps, GetTermFlg, RingGetRead, IsAllOutTerm, UpdateCnt, a
  `ret = Loop(); return ret;` caller copy, `(skip = SFCON_..()) != 0`.

### CRI SWAR kernels pass 7 (continued): 16x16 V2 residue = case-0/1 colours only; 8x8 4p target colours read; nothing applied, nothing flipped (2026-09-12)
- **mpv_mcy `MPVMC16_OneRefV2_TuneC` 225w = 78 differing lines, ALL in the prologue (`stmw r17` vs `r18`, frame 0x50/0x40), case 0
  (45: pure register renames of the same instruction stream, `i` r10 vs r9, w0/a0 r17/r18 vs r25/r28, ...) and case 1 (28: renames plus
  the `and r19, x0, m1` slot and `srwi` one instruction apart); cases 2 and 3 are byte-identical. `chaitin.py --check` is IDENTICAL on
  our dump, so this is a vid-order question of case 0's own-local webs (case 0 = the first webs = declaration order; cases 1-3 = @temps
  numbered by first definition) — a chaitin.py replay over the 15 own locals' orders is the next step, not permutation builds.
- **mpv_mc `MPVMC08_OneRef4p_TuneC` 72w:** target colours ctr-temp r0 / b0 r0 (shared: the `li r0,8` does not interfere with a body
  value), d r3, stride r4, s0 r5, s1 r6, i.e. exactly ONE pixel node is coloured before the four loop pointers (a pass-2 survivor with a
  vid above them) and the pointers are declared `d, stride, s0, s1`; ours colours stride r0, d r3, s0 r4, s1 r5 with seven pixel own
  locals (a2..a7, b5, total degrees 30-50) surviving pass 1. Declaring the pixels before the pointers (e1-e4) moves them to r0,r3..r7
  and the pointers to r8-r11 (72w); `cnt` counter forms (`for (cnt = 8; cnt > 0; cnt--)`, `while (cnt--)`, `do..while (--cnt)`) leave
  the `li` a backend temp coloured after the pointers (74-75w); pointer order alone (d1-d4) 71-74w. The target's load order (a0, b0,
  dcbt, a1, b1, .., a6, b6, **a8, a7**, b7, b8) and the 16x16's (b1, a2, b0, a1, b2, a3, **b5**, b3, a6, ..) hoist the inner-sum
  operands of a LATER word/pair above address order where ours keeps address order: a scheduler-window difference on the same raw order,
  unexplained (rule of thumb from pass 6 stands: the raw order is the lever, but the window is wider in the original build).
- Not started this pass: mpv_mcy H2 225w, mpv_mc V2 73w, mpv_mc H2 436w. objects.py untouched; no unit flipped; 111 not re-run (no tree
  source edit). Harness ~/.cache/cri_swar7 deleted.

### CRI pass 30 (adx_dcd5 ADX_DecodeMono4 39 -> 0w, pure C: the scale as a `Sint16` own local + a redefinition blocker + the table value as an own local; Ste4AsSte 118w / Ste4AsMono 180w read in the model, unchanged; adx_baif AIFF_GetInfo 170w read, unchanged; 2026-09-11)
Harness ~/.cache/cri30/ (deleted): `mk.py`/`try.sh` = variant.sh wrappers, `ste.py "decl order" --sc16 --q --blk 'R:a=>b'` = Ste4AsSte
variant generator, `model.py`/`model2.py` = chaitin.py replays with permuted own-local vids. Dumps of ~12 probes (ra.py). Unit flags untouched
(adx_dcd5 2/4 functions identical, not flipped). The tree was reset by a history rewrite at 01:07; the Mono4 edit was re-applied from the record.

**Mono4 fixed (three facts, each read off a dump, each necessary; `variant.sh` 39 -> 24 -> 0):**
- **A conversion is emitted INTO the destination variable only when it is the root of the RHS and its operand is not itself a conversion.**
  `sc = (Sint16)(X)` is `ETYPCON long(ETYPCON short(EADD))`: the inner conversion makes a temp (`extsh r64, r63`), the outer one is a
  no-op, the assignment is `mr sc, r64` and backend copy propagation deletes the own local (pass 26's finding). `Sint16 x; x = X;` is
  `extsh x, r63` (into the variable), `Sint32 y = x16;` is `extsh y, x16` (into y), `d = src[0]` is `lbz d; extsb d, d`. A two-def own local
  (`sc = X; sc = (Sint16)sc;`) keeps the `mr` (never coalesced, f3 probe), and the frontend folds it back anyway unless blocked.
- **The peephole-forward pass (backend-01) turns `extsh rD, rS` into `mr rD, rS` when rS was defined by an `extsh` in the same block**, and
  copy propagation then substitutes rS. So `Sint16 sc; sc = ((s ^ key) & 0x1FFF) + 1;` (def `extsh sc, r66` into the own local) followed by
  the frontend's hoisted `@N = (long)sc` in the inner-loop preheader (same block B5) gives `mr @N, sc` -> the loop uses `sc` itself: the scale
  is an own-local node with ONE extsh. Precondition: the frontend must not substitute sc's def into the hoist (`Sint16 sc` alone = 39w
  again, the hoist becomes `(long)(int)(short)X` = the backend temp; pass 26's 61w was the cast kept on the RHS). Blocker used: a
  REDEFINITION of an RHS operand between the def and the hoist — `key = *scl; sc = ((s ^ key) & 0x1FFF) + 1; key = sadd + key * smul;
  *scl = key; *scl = *scl & 0x7FFF;` (a store between does not block: the RHS has no load; `*scl &= 0x7FFF` is equivalent). Same bytes for the
  scramble update (`add; sth` — the extsh before a `sth` is dropped by copy propagation).
- **The table value `AdxQtbl[d & 0xF]` is an own local `q` declared LAST, defined before the `out[0] = t` store** (the store blocks the
  substitution because the RHS has a load; defined after the store it is substituted, 30w). As the lowest own-local vid it is coloured last and
  takes the dying nibble's register (`lwzx r26, r28, r11` with d = r26) — as a backend temp it is coloured third in L1 and takes r29. The
  multiply is `q * sc` (`mullw r12, r26, r31`: table first).
- Declaration order `i, l2, l1, j, s, key, sc, d, t, q` (L2 order i r10 > l2 r11 > l1 r12 > sc r31 > sadd r30 as pass 26 predicted; with
  `l1, l2, i` first l1/l2 swap, 24w).

**Ste4AsSte 118w / Ste4AsMono 180w (not changed; the same three lessons give 113-115w, the model says the graph structure differs):**
- Target colouring read off the bytes: sadd r0, smul r11, scl r12 (L3), c2-ext r10 / c1-ext r9 IN PLACE (the param registers, before the
  stmw), then new callee-saved in the order l2 r31, r2 r30, r1 r29, l1 r28, i r27, d r26, dr r25, sc_l r24, sc_r r23, AdxQtbl r22, s r21, t r20,
  nblk r19; the table values `lwzx r26 (d's reg) / lwzx r25 (dr's, in place)`, the second s in r23, second key r25, `mullw r26, r26, r24` =
  `q_l * sc_l`. Ours: AdxQtbl (backend temp r107, L2, coloured first) takes r9 and the two `d >> 4` / `dr >> 4` temps (exactly 29 neighbours
  each -> L2) take r10 before the c-ext @temps, which then go to r28/r29 (`extsh r28, r9`), and every own local shifts (14 callee-saved,
  stmw r18). chaitin.py `--check` IDENTICAL on ours; permuting the own-local vids in the model (model.py, 400 random orders; model2.py grid over
  nblk/AdxQtbl vid positions) never gets below 12/15 wrong -> the target's graph has different LEVELS, not a different order: (a) AdxQtbl must be
  L1 (degree < 29 at its scan) yet be coloured before s/t/nblk, (b) t (39 neighbours here) and nblk (76) must be coloured after AdxQtbl,
  (c) the `d >> 4` temps must be < 29. `nblk = nfrm / 2` as an own local is L2 or L3 depending on its declaration position (declared late =
  stuck in iteration 2 -> L3 -> r0); `i < nfrm / 2` in the condition becomes a frontend @temp (@65, L2, r29); `return i * 2` at the end
  (semantically wrong, probe only) drops 4 bytes and 37 words = the nfrm ghost (r33) matters. Not found: which node the target lacks. Open.
- Ste4AsMono: same class, not dumped.

**adx_baif AIFF_GetInfo 170w (unchanged):** the target keeps the header ckid/cksz as variables (`mr r27, r30; rlwimi r27, r31, 24, 0, 7`:
the LE32's last OR written into the own local through the or->rlwimi peephole; a copy from a backend temp into an own local is never
coalesced) and swaps the size into a TEMP before the FORM/AIFF checks (`rlwinm r12, r28, ..` x4 then `subi r10, r12, 4; add r10, r8, r10`
after the checks), i.e. `cksz` is not reassigned; the loop's cksz likewise (`rlwinm r29, r27` = one CSE'd SWAP32 used by the `< 0x12` test
and the default step). Probes: SWAP32 at the uses in header+loop 170 -> 154w but +12 bytes (the loop's swap is emitted twice: once in COMM
at `cmpwi 0x12`, once in default — the frontend did NOT CSE the two macro uses across the switch, unlike pass 13's macro CSE), header ckid
still substituted into its compare (`||`-joined checks 156w, `p`-relative loads with `p += 12`, `buf += 8`: no change). Open: what keeps the
header ckid a variable (its only use is the FORM compare two statements later) and the 8-byte size gap (the two extra `clrlslwi 16,8` 16-bit
reads at COMM: ours `rlwimi 8,16,23`).

### CRI pass 31 (mpv_umc OneReadMb 56 -> 48w APPLIED (pure C: `yhx = vx & 1` / `chx = cvx & 1` before the fn table loads); mps_lib 2w / adx_tsvr 2w / sfh_main 6w unchanged; nothing flipped; 2026-09-12)

Harness ~/.cache/cri31/ (deleted at the end): try.sh (variant.sh + md5 of the object + side-by-side lines), sweep.py (constrained
statement-order sweeps of the OneReadMb vector block, 70 + 168 orders), ra.py dumps of v2 (ra_v2) and MPS_Create (ra_mps). Tree edits:
src/lib/mpv_umc.c `mpvumc_OneReadMb` body + comment only.
- **mpv_umc `mpvumc_OneReadMb` 56 -> 48w applied**: `ypos; yhx = (Uint32)vx & 1; fn_y = tbl_y[vy & 1][vx & 1]; cvx; cvy; cpos; chx =
  (Uint32)cvx & 1; fn_c = tbl_c[cvy & 1][cvx & 1]; chx &= mcflag; yhx &= mcflag;` (locked ninja + bytecmp 15/16, 48w; .bss order OK).
  **New frontend rule (read off five single-variable probes, objects compared by md5): a range-split web whose single use is the variable's
  own second definition (`yhx = vx & 1; .. yhx &= mcflag`) is KEPT (emitted at its statement, `rlwinm yhx, vx` + `and yhx, yhx, mcflag`)
  when a LOAD statement (`fn_y = tbl_y[..][..]`) sits between the two definitions; a store (`mc->stride = cpitch`) between them does NOT
  keep it (chx with cpos + store between: still sunk, `rlwinm t; and chx, t, mcflag`).** So the target's `clrlwi r24, vx, 31 .. and r24,
  r24, r8` / `clrlwi r23, cvx, 31 .. and r23, r23, r8` pairs mean the vendor wrote each `& 1` before its table load. `fn_y = tbl_y[vy &
  1][yhx]` compiles to the SAME object as `[vx & 1]` (the peephole folds `slwi yhx, 2` of `clrlwi vx` into `clrlslwi r12, vx, 31, 2`),
  so the target does not distinguish the two spellings. .bss order of mpvumc_oneref_y / mpvumc_oneref = the order of the fn_y / fn_c
  statements (tbl_y/tbl_c are single-use and substituted), so fn_y must be referenced first.
- **OPEN 48w = the pre-RA scheduler, not the RA.** The debugger's pass list for this function is backend-00 initial .. 06 peephole-forward,
  **07 after-scheduling (PRE-RA)**, 08 peephole-forward, 09 before-regalloc, 10 regalloc, 12 peephole, 13 after-scheduling (post-RA).
  In backend-06 `rlwinm r46 = vx & 1` (stmt 747) precedes `lwzx r47 = fn_y` (748); backend-07 issues them `lwzx r47; rlwinm r46` (one
  cycle, the load first), so vx (r40) interferes with fn_y and keeps its 33 neighbours (level 2, r6); in the target `clrlwi yhx` is
  above the load and vx dies into fn_y's r25 (chaitin.py --check IDENTICAL on ours). The pre-RA scheduler is a top-down list scheduler
  with a cycle model (fillers: `mr r33, r4` / `mr r3, mc` / the row-base `addi`s pulled into the lha/mullw stall slots at the block top;
  loads are issued before the first store; zero-successor instructions -- `add ypos`, dead `rlwinm`s, `and yhx` -- go last in statement
  order). Not moved by: 70 orders with ypos/cvx/cvy first, 168 orders with `yhx;fn_y` and `chx;fn_c` adjacent and the masks last (best 48w
  x6: ypos first, then yhx/fn_y, cvx, cvy, cpos, chx/fn_c), an empty `asm { }` between yhx and fn_y (removed, identical object), the
  first def as `asm { rlwinm yhx, vx, 0, 31, 31 }` with `register` yhx/vx (identical object: asm instructions are scheduled like C ones
  pre-RA too). Pass-27's "vid-order permutations cannot reach the colours" stands; the lever has to give `rlwinm yhx` a higher pre-RA
  priority than the load (an in-block consumer) or delay the load's readiness -- none found in C.
- **Pre-RA scheduler facts measured on OneReadMb B1 (backend-06 -> 07, all with ra.py dumps, objects by md5):**
  (a) every load is ordered before every LATER store and after every EARLIER store (a `fn_y` load written after `mc->stride = ..`
  never crosses it, r1/r2 probes; `static` on the table changes nothing; no type-based disambiguation). (b) A load's issue slot is set
  by the stores that follow it: with the four `mc->` stores present the `lwzx fn_y` is pulled to slot 50 of 72 (before `rlwinm yhx`);
  with no store in the block it sinks to 56/68, with any ONE of the four kept it sits right before that store, and with the stores in
  the source order src2, src, dst, stride it sits at 60 -- in all of those `rlwinm yhx` (slot 40) precedes it. So the pick is a
  property of the store list after the load, not of the yhx statement. (c) Height is NOT the whole priority: after `lwz vx/vy` the
  first pick is `srawi vx>>1` (a two-add sinker chain), the `lis` table bases go to the block top, `mr r3, mc` fills a mullw stall.
  (d) No register-pressure back-off: 1-4 extra values live across the block (g1-g4) leave the B1 order unchanged.
  (e) 168 statement orders (chk.py: pre-RA slot of `rlwinm yhx` vs `lwzx fn_y` + words): 42 orders have the rlwinm first, all of
  them 57w+ (ypos or cpos written after the table loads, so `vx >> 1` / the ofs reloads move and vx still interferes with fn_y's
  neighbours); the 48w orders all have the load first. (f) `#pragma scheduling off` (both schedulers off, 87w) gives vx degree 24 =
  the target's, fn_y r25, cvy r7, yhx r24, chx r23, cpos r29, ypos r28 (7/10 target colours) but vx r8 / vy r7 / cvx r8; no order of
  the 168 reaches more than 7/10, so the target's graph is a SCHEDULED one, not statement order. (g) Reusing `vx` as the fn_y
  variable (`vx = (Sint32)tbl_y[..][..]`, call through a cast) is 83w -- not the vendor's shape. Lever left: something that gives
  `vx & 1` a successor the scheduler ranks above the four-store chain without emitting an instruction; none found in C.
- **mps_lib `MPS_Create` 2w** (`li r4, -1` / `addi r0, r3, mps_obj@l` swap at the tail of the last block): identical through backend-07
  (pre-RA), the swap appears only in backend-13 (post-RA). Post-RA `addi r0, r3, ..` must follow `mr r3, r31` (r3 is the return
  value being written -- anti-dependence on r3), so its slot is fixed one behind the `mr`; the target has the `li r4` in that slot.
  12 variants (pointer-stepping stores, the `-1` in a local, the fn-pointer stores moved, return shapes `return mps` / `return
  (MPS)&mps_obj[i]`, `#pragma scheduling off` = 72w) all keep the swap or explode; left 2w.
- **adx_tsvr `adxt_nlp_trap_entry` 2w** is a colour residue (`lha r4` vs `lha r0` in the join block after ScanInfoCode): the target has
  r0 and r3 both blocked there, ours only r3 (= n2); nothing else lives in r0 across that block in any spelling tried; left 2w.
- **sfh_main `SFH_AnlyElemSmpHz` 6w** = the M4 class (target `rlwinm/rlwimi x3/stw` swap-store, ours `stwbrx`; sizes 348/332); the file
  header and CRI pass 27 already record every spelling, `#pragma peephole off` (10w, loses the inlined search's displacement fold)
  and the asm chain; nothing new tried this pass.

### CRI mwsfdcre pass 5, continued (the "Applied / negative / residue" part re-appended after the 01:07 tree reset; the source edits were re-applied from the record and rebuilt: CreateSfd 126w, CalcWorkSfd 4w, 8/10, not flipped; 2026-09-12)
- **Applied after the pool fix (each step verified with variant.sh, then the locked ninja; all pure C, no pins):**
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

### CRI pass 33 (cftyp422_ppc 6/8 identical, 222w -> 105w: .bss order + .rodata/cnvStatic fixed, Init 24 -> 9w, table makers 0w, Y84C44 179 -> 96w; 2026-09-12)
Harness ~/.cache/cri33/ (regmap.py = per-role register map from `variant.sh --all`, y84.py / perm5.py declaration-order permutation drivers, model_search.py = chaitin.py single-edit search with K=28, tree1..6.c = tree snapshots). Kit only; variants and ra dumps deleted.
- **.bss is NOT definition order** (probe: the six objects declared in the target's order leave the layout unchanged); it is
  TU first-reference order in codegen, as pass 16 and the mwsfdcre pass 5 found. Lever applied (tagged, `cftyp_bss_order`): a
  never-called global function defined between cnvDynamic and cnvStatic that stores to cr_r, cr_g, cb_b, cb_g, y in that
  order; strip_unused.py drops it from .text (it is not in sym_map.tsv), its references fix the .bss layout (target offsets
  +4/+0x404/+0x804/+0xc04/+0x1020/+0x1420 all match now). The original's cnvStatic is C and references the tables through its
  gqr_save-based pool; the asm transcription carries the pool immediates, so nothing in our code references them before Init.
- **Init 24 -> 9w, pure C**: the five induction pointers are OWN locals declared after `i` (`Sint32 i; Float32 *py = y,
  *pcb_g, *pcb_b, *pcr_r, *pcr_g;` in the store order, `*py++ = ..`): i (highest own id) is coloured first and takes the
  dead .bss base's r5, the pointers r6..r10; the index form makes them IV @temps above i (i r10, cr_g's IV reuses r5). The
  `stw CFT_dummy`/`li r0, 0x100` schedule followed. Residue 9w = FPR order: the target's y product is coloured LAST (f8,
  a variable vid below the hoisted 1.164 f7 / magic f6 literal loads); ours is an expression temp created after them (f6).
  A `Float32 v` local is forward-substituted (E/F/G/J/K/L/P/Q variants: `register`, `*py = v; py++`, a cast, `(void)v`, an
  inlined helper's return); the two-def `v = (Float32)(i - 16); v = 1.164f * v` keeps v a node (product f8, conv f2 =
  target) but generates the conversion first, so the magic/1.164 literal ids swap (8w). Left.
- **Table makers 9/8/8 -> 0w and .rodata 0x88 / cnvStatic 0w, pure C + one tagged pool reference.** The three
  `CFT_MakeArgb8888Alp*Tbl` diffs were one scheduler slot: the cb/cr sub-table pointers must derive from the `y` COPY of
  `tbl` (`cb = (Float32 *)((Uint8 *)y + 0x1000)`, `CFT_MAKE_CHROMA_TBL(y)`), not from `tbl`: the in-order dual-issue
  scheduler delays an `addi` whose input is a copy-of-a-copy by one cycle, which is exactly where the target has it.
  .rodata: the version string is a named `static const Char8 cft_version_str[]` (first .rodata object, the `CFT_version`
  pointer initialised from it), and cnvStatic's asm addresses its literal pool as `cft_version_str + 0x50@ha/@l` (MWCC's asm
  parser accepts `symbol + constant@ha`, bytecmp resolves the relocation by address); a `static const Float32` scalar for
  the same purpose is constant-propagated into a literal and its object deferred to the pool end (0x88 -> 0x8c), and a
  global `const` is stripped by strip_unused.py (".text still references removed symbol"). Tagged COMPILER-DIFF.
- **Y84C44 179 -> 114w, pure C (semantic bug + variable kinds).** The 179w were a register permutation over IDENTICAL
  instruction streams except ONE real bug hidden among the register diffs: the chroma tile pointer steps `cskip * 8` words
  per tile row (`slwi r23, r11, 5`), not `cskip` (ours `slwi .., 2`). Then, reading the target's colours as the Chaitin
  order (volatiles lowest-free first, callee-saved handed out r31 downward for the spill-candidate picks, the low-degree
  leftovers coloured last and REUSING handed-out callee-saved from r21 upward), the source shapes that reproduce them:
  (a) ONE `cnt` variable for both loops (`cnt = ywidth / 8` then `cnt = ywidth / 2 / 4`): it spans both loops, is coloured
  early and takes ybuf's r4 in place (`addze r4, r4`) in both; separate `cnt`/`ccnt` gave r28/r29. (b) The y-row steps are
  BYTE offsets kept in own locals: `yskip = ywidth * 3 / 8 * 8; dskip = (width - ywidth) / 8 * 32; y0 = (Float64 *)((Uint8
  *)y0 + yskip)` — with element counts (`y0 += yskip`) the loop-carried value is the hoisted `slwi` scale temp (high vid,
  coloured before every own local: r9/r10), with byte offsets the named locals themselves carry the loop (r21/r22, coloured
  after the y pointers). (c) `ywidth * 3` written twice (y3 offset and yskip) is one CSE temp (r10, volatile), not a local.
  (d) Loop-2 row pointers are assigned cb-first (`cbp1 = cbp0 + cw; cbp2 = cbp0 + cw * 2; cbp3 = cbp0 + cw3; crp1 ..`, no
  o1..o3 locals — the offsets come out as the same hoisted `slwi` temps) and declared crp3, crp2, crp1, cbp3, cbp2, cbp1:
  the spill-candidate ties go to the highest vid, and BOTH the declaration order and the assignment order move the group
  order. (e) Loop-1 own locals declared in the target's colour order: `ywidth, n, y3, yskip, dskip, y2, y1, d, y0, yw2, hblk,
  cnt, i` (low-degree own locals are coloured in DECLARATION order, first declared first; `n`'s position is irrelevant — the
  `while (n-- > 0)` counter copy is coloured before every own local). Residue 114w: two swaps — target crp0 r3 / c r5 /
  crv r6 / cbp1 r9 / cbv r10 vs ours c r3 / crv r5 / crp0 r6 / cbv r9 / cbp1 r10 (crp0 must be coloured before c; no
  declaration order, `register`, statement order or cbwidth/o-expression spelling moves it — all 120 permutations of the
  five loop-2 top declarations tried), and ywidth r9 / n r11 vs ours r11 / r9 (ywidth as a CSE'd `src->ywidth` breaks the
  in-loop y1 offset: the stores alias). chaitin.py does not replay this function (65 divergences), so no model help.
- **Y84C44 114 -> 96w (tree): cr rows first, o1..o3 locals, yw3 local, cbp0 declared first.** Corrections to the bullet
  above (my regmap labels had cb/cr swapped): the target keeps the *cr* row pointers in volatiles (crp1 r9, crp2 r11,
  crp3 r12) and the cb ones in r31..r29, so the rows are assigned cr-first and declared `cbp3, cbp2, cbp1, crp3, crp2,
  crp1` (spill picks tie to the highest vid = first declared). The row offsets are NAMED locals `o1 = cw * 4; o2 = cw * 8;
  o3 = cw3 * 4` and the y3 offset a named `yw3 = ywidth * 3` (declared right after ywidth): a named local as the left
  operand gives `add rD, rOFF, rPTR`; a CSE'd expression gives the swapped `add rD, rPTR, rTMP` (-8w). Loop-2 top
  declarations `cbp0, crp0, crv, cbv, c` (6 of 120 orders tie at 96w).
- **chaitin.py is EXACT on this function with K = 28 and r0 removed from the volatile list** (default K = 29 diverges at
  the first stuck pick): the asm `li r0, 8/4` + `dcbz d, r0` keep r0 live through both loops, so every loop node has r0 as a
  precoloured neighbour and there are only 28 colours. Use `chaitin.K = 28; chaitin.VOLATILE = [3..12]` for any function
  with an asm-pinned r0. With that model the 96w residue is ONE interference edge: crv (and cbv) have residual degree 28 at
  the moment cbp0 is picked (r0 + c + crp0 + cnt + the 24 `mr` ghosts of the eight packed words, 3 per word); the target
  needs 27 so that crv/c/cnt simplify and crp0 stays last (crp0 r3, cnt r4, c r5, crv r6, then crp1 r9 / cbv r10). Deleting
  any single ghost edge in the model reproduces the target's loop-2 colours; no source spelling found that drops one ghost
  (named accumulators, re-associated terms, crv-term-first all change the instruction stream). Loop 1: the target has
  ywidth r9, yw3 r10 and the unroll-remainder counter copy r11 (`mr r11, r4; andi. r11, r11, 3`); ours has the copy at
  r9 because the copy is a loop-transform temp (vid 154, coloured before every frontend local). Every loop form
  (`while (n-- > 0)`, `for (n = 0; n < cnt; n++)`, down-counting, separate loop-2 counter) produces the same copy temp;
  computing the count inside the outer loop or using `src->ywidth` in the loop reloads (asm stores are a barrier).
  Frontend `n` is r65 (dead after the transform); the target's r11 would be n itself if the transform had reused it.
- Flags: `lib/cftyp422_ppc.c` stays False (6/8 identical, 105w: Init 9w FPR order, Y84C44 96w = one crv/cbv edge +
  the loop-1 counter-copy vid); objects.py untouched by this pass. `.data` 0x8/0x4 tail pad still open (the target has 4
  zero bytes after `CFT_version`; ours pads to 8 by alignment). Tree = ~/.cache/cri33/tree6.c.

### CRI pass 32 (sfd_adxt AdjustSync 47 -> 0w pure C, ExecServerSub 112 -> 88w at target size; cri_cvfs cvFsGetFileSize 36 -> 32w, cvFsOpen 56 -> 18w; sfd_tst SFTST_Calc 79w unchanged; no flip; 2026-09-12)
Harness ~/.cache/cri32/ (deleted at the end): `try.sh <unit> <abs variant.c> <FUNC>` (variant.sh word counts only), ra_* dumps,
v*.c / a*.c / e*.c variants. No pins, no pragmas; all edits are declaration order / helper boundaries.
- **The colour rule, restated from chaitin.py (it is what pass 25 mis-read): a node takes the lowest-NUMBERED free register among
  r0, r3..r12 and the callee-saved registers ALREADY handed out, else a NEW one from r31 downward.** Consequences used below: a
  value coloured right after a group of higher-level values reuses the LOWEST dead one of their registers, not the most recent;
  "X needs an rN-coloured neighbour" is only true when rN is below every other free register.
- **The LEVEL of an own local depends on the declaration order too.** The removal scan runs in ascending id = reverse declaration
  order, and each removed node lowers its not-yet-scanned neighbours' degrees. A local with ~30 neighbours therefore stays in
  level 2 only when its removable (level-1) neighbours are declared BEFORE it (higher id, scanned later). sfd_adxt AdjustSync:
  skipbyte (31 total) stays in level 2 with lim/frmbyte declared before it; declared after them it drops to level 1 and the whole
  colouring shifts (a1 variant: skipbyte r23, parameters r27..r24).
- **sfd_adxt `sfadxt_AdjustSync` 47 -> 0 (pure C):** declaration order `lim, frmbyte, skip, skipbyte, nch, endflg, tim, vstart,
  astart, sfreq, ofs, n, dmy, vflg, diff, wk, ins, cnt`. Level 2 colours in that order (skip r30, skipbyte r29, nch r28, endflg
  r27, parameters r26..r23); in level 1 the @temps p/ofs come first (r30/r28 = the dead skip/nch registers), then lim r22 /
  frmbyte r21 (new), then tim, vstart, astart, sfreq take the lowest free each: r21, r22, r27, r29 = the target. No skip edge
  and no `li` placement question: the target's `li r30, 0` after the `bl` is exactly ours once the colours agree. The else
  branch's silence count is its own variable `cnt` (declared last, r21 = frmbyte's dead register): as a second web of `n` it
  interferes with frmbyte (the then-path `n = len / frmbyte * ..` keeps frmbyte alive) and lands in r24.
- **sfd_adxt `sfadxt_ExecServerSub` 112 -> 88 (sizes now equal 0x428):** (1) the target loads `SFADXT_WK(sfd)->adxt` BEFORE the
  `que_wr == que_rd` test (hoisted into the compare block): `adxt = SFADXT_WK(sfd)->adxt;` precedes the `if`. (2) The
  `nsmpl = ADXT_GetNumSmpl(adxt)` copy bounces (`mr r0, r3; mr r3, adxt; mr r24, r0`, +4 bytes) as an own local of the real
  function; as a local of an inlined helper (`sfadxt_WriteTotSmpl(sfd)`, the whole tail) it propagates into the @ret and the
  copy is the single `mr r24, r3` (the pass-20 helper-local rule, now seen on a non-inlined caller too). (3) `void *obj` +
  `SFD sfd = obj` (pass 12) puts sfd above len; the declaration order `err, len, bufout, bufin, tst, stat, adxterr, adxt, wk`
  colours the middle block like the target (bufout r28, bufin r27, tst r27, stat r28, adxterr r26, adxt r25, wk r24).
  Left (88 words, all renames): sfd r31 / err r30 (target has sfd in level 3: ours 28 at removal, one neighbour short; the err
  range-split @432 is above sfd in level 2 either way), and the ahdr / svrfreq / tail blocks: target ahdr r25, adxt r24, wk r25,
  nsmpl r24, i.e. r28..r26 are blocked there although stat/tst/adxterr are dead — the target's ahdr-block values interfere with
  something coloured r28/r27/r26 that ours does not have (a range-split copy of adxt or wk live across the ahdr block is the
  candidate; ours reuses the lowest free r28/r29). Not closed.
- **cri_cvfs cvFsGetFileSize 36 -> 32w, cvFsOpen 56 -> 18w (pure C):** (1) cvfs_AllocObj declares `obj` before `i`: the
  later-declared helper local ranks higher, so `i` is coloured first and takes r3, `obj` r4 (34 of cvFsOpen's words). (2) The
  first device search is one inlining level deeper than the two SearchDev copies (`cvfs_WantsDevForm(tbl, name, len)` =
  `OptFn(FindDev(..)) == 1`, called from ResolveDev): cloned after them, its `i`/`dev` get the lowest ids and the second copy's
  `i` takes r26 like the target. (3) Read off the dumps with the correct colour rule, the target's full order is
  {name, vtbl chain, i2, i3} > i1 > dev1 > len1 > dev2 > {len2, len3}: the second SearchDev copy's `dev = tbl` copy (r23) is
  coloured AFTER the first search's strlen result (r23, a late range-split copy @139x created after every clone local), while
  the same copy's `i` is coloured before the first search. No clone-local form gives that (a clone's `i` and `dev` are adjacent
  ids); `tbl++` on the parameter instead of `dev = tbl` makes the copy at clone entry, before the strlen (wrong shape, 33w).
  The coalesced vtbl chain ranks by its LOWEST-id member (the last clone's return temp @1387), so it sits below every clone
  local; raising it needs the third search's return temp created before the second search's locals. Also open: pdev r28 / tbl
  r27 (both level 2, tbl is a later-created copy leader @1391 above pdev; pdev at 28 needs one more level-2 neighbour or ghost).
- **sfd_tst SFTST_Calc 79w:** not touched (box spent on the units above). Re-read of the sprintf block with the lowest-free
  rule: the nine mutually interfering values take r30..r21 in colouring order; target order hlp.lo, hlp.hi, mt_max.lo,
  mt_max.hi, mt.lo, MulDiv, out.lo, out.hi, mt.hi (mt.hi LAST, r21); ours colours mt.hi FIRST (r30) and the other eight in the
  target's order. So the single residue of that block is mt.hi's rank: ours has it above the other eight (highest id or a
  higher level), the target below all of them.

### CRI flag search: block split (no MWCC 2.4.7 option or pragma controls the >100 split — it is an immediate `cmp ..,100` in the statement codegen loop; nothing applied; 2026-09-12)
Harness ~/.cache/cri_flags/ (kept small: `probe.sh "<cflags>" [src] [func]` = mwcc-debugger GC/2.6 run with an arbitrary flag string,
prints the backend-00 block sizes; `cands.py`/`cands2.py` + `run.sh`/`run2.sh` = the candidate lists; `results.txt` = every result;
`mw27.dis` = `objdump -d -M intel` of GC/2.7 mwcceppc.exe; `help.txt` = `mwcceppc -help`). Probe TU `probe.c` = 51 x `x = x*3+1;`
(102 emitted instructions) then `d[0] = x; d[1] = x+1;`: split = `B2=102 B3=4`, unsplit would be one block of 106.
- **Command-line sweep (75 variants, all SPLIT):** `-O4` / `-O4,s` / `-O3,p` / `-O2,p` / `-O1,p` / `-O0`; `-opt level=4` alone, `+peephole`,
  `+schedule`, `+peephole,schedule`, `-opt all`; on top of `-O4,p`: `-opt noschedule/nopeep/nospeed/space/nocse/nodeadcode/nodeadstore/
  nolifetimes/noloop/noprop/nostrength/nointrinsics`; `-schedule off/on`; `-inline off/on/all/auto,deferred/auto,level=0/auto,level=8/
  noauto`; `-func_align 8/32`; `-pool off`; `-str readonly,noreuse` / `readonly,pool`; `-common on`; `-RTTI off`; `-nosyspath`; `-proc
  750/generic/7400/603`; `-fp_contract off`; `-fp soft`; `-use_lmw_stmw off`; `-sdata 8 -sdata2 8`; `-g`; `-sym on`; `-enum min`; `-char
  unsigned`; `-Cpp_exceptions on`; `-vector on`; `-profile on`; `-model other`; `-strict on`; no `-nodefaults`; `-little`; `-align mac68k`;
  `-r`; `-msext on`; `-noprecompile`; `-once`. Rejected by the driver (no object): `-ipa file`, `-ipa function`, `-unroll`. The only
  shape changes: scheduling OFF (`-O3,p`/`-O2,p`/`-O1,p`/`-opt level=4[,peephole]`/`-opt noschedule`/`-schedule off`) gives `B1=100
  B2=6` (no empty entry block, the split one statement earlier — still split); `-O0` gives `B1=102 B2=3`; `-profile on` adds an empty block.
- **Pragma sweep via `-pragma '...'` (76 variants, all SPLIT):** `scheduling off/750/7450`, `optimization_level 0..4`, `peephole off`,
  `global_optimizer off/on`, `opt_common_subs/opt_dead_assignments/opt_dead_code/opt_lifetimes/opt_loop_invariants/opt_propagation/
  opt_strength_reduction off`, `opt_strength_reduction_strict on`, `opt_unroll_loops on/off`, `opt_vectorize_loops on`, `optimize_for_size
  on/off`, `optimizewithasm on`, `pool_data off`, `merge_float_consts off`, `inline_depth(8)`, `inline_max_size(1000)`,
  `inline_max_total_size(100000)`, `auto_inline on`, `dont_inline on`, `always_inline on`, `inline_bottom_up on`, `explicit_zero_data on`,
  `strict_conditional on`, `gen_fsel on`, `no_register_save_helpers on`, `register_coloring off`, `fp_contract off`, `far_data on`,
  `switch_tables off`, `volatile_asm on`, `profile on`, `sym off`, `traceback off`, `longlong on`, `min_enum_size int`, `unsigned_char on`,
  `ANSI_strict on`, `only_std_keywords on`, `require_prototypes on`, `check_c_src_compat on`, `defer_codegen on`, `direct_destruction on`,
  `suppress_init_code on`, `altivec_model/altivec_codegen on`, `processor 750/generic`, `code_seg text`, `ipa file/function/off`, and the
  unknown-pragma guesses (`loop_unroll`, `unroll`, `opt_loop_unroll`, `unroll_loops`, ... = warnings only). Driver errors (no object):
  `ppc_unroll_speculative`, `ppc_unroll_instructions_limit`, `ppc_unroll_factor_limit`, `interrupt`, `section text`, `precompile_target`.
- **Why nothing can work — read off the GC/2.7 binary (`mw27.dis`, statement codegen loop at 0x433726..0x433c91, `switch (stmt->type)`
  through the table at 0x5a763c):** for ST_EXPRESSION (0x433749), ST_IFGOTO (0x4337e5), ST_IFNGOTO (0x433845), ST_GOTOEXPR (0x4338a5),
  ST_GOTO (0x433988), ST_RETURN (0x433a15) and ST_SWITCH (0x433a89) the handler does `if (pclastblock->pcodeCount == 0) pclastblock->line =
  stmt->line; if (pclastblock->pcodeCount > 100) { b = makepcblock(); pcbranch_link(b); }` before generating the statement
  (`cmp WORD PTR [edi+0x28],0x64; jle +0xc; call 0x4dd020; push eax; call 0x4eadb0`; `pclastblock` = ds:0x5eea88, `pcodeCount` = the s16 at
  +0x28 the debugger reads). The 100 is an IMMEDIATE; no global, option bit or pragma variable is read anywhere in the check. ST_LABEL,
  ST_NOP and ST_ASM (0x433c1f, the inline-asm statement) have no check. The identical 7-site byte pattern `66 83 7f/7e 28 64 7e 0c e8` is in
  GC/1.3.2, 2.0, 2.5, 2.6 and 2.7 (every 2.4.2/2.4.7 build we have); GC/3.0a3 (4.1) has 0 sites (irrelevant: 4.x loses every CRI unit).
- **Exact rule (probes g/h):** the check runs BEFORE each statement and never inside one: 50 statements (100) + one 60-instruction
  statement = ONE block of 160 (probe_g); 51 statements (102) + the same statement = 102 | 61 (probe_h). Frontend forwarding decides the
  statement sizes: in the current 4p body the single-use `p0..p7` assignments emit nothing (their sums are forwarded into the `d[k]`
  statements: line 52 = 24, 53 = 20, 70 = 24, 71 = 20 instructions), the dual-use pixel loads stay 1-instruction statements; count before
  `d[16]` = 87, after = 111 > 100 -> split before `d[17]`.
- **Consequence for the target's 124-instruction body:** its block ends with the `if (i == 7)` IFNGOTO (`cmpwi r4,7 .. bne`, the loop
  itself is `mtctr/bdnz` with `i` kept in r4 for the test), which is a checked statement, so at codegen time the block held <= 100
  instructions BEFORE the `if` and no post-codegen pass in our dumps ever merges the split blocks (B3/B4 stay separate through backend-14).
  Ours emits 136 initial instructions for the body (134 before the `if`; peephole-forward only turns `rlwinm/or` packs into `rlwimi`,
  -12; the +9 are coalesced `mr`s), so the target's source emitted <= 100 initial instructions for the same 122 final ones: either the original's initial
  PCode was >= 22 instructions more compact than ours (a form some backend pass expands — not seen in any of our passes) or its statement
  set differs (e.g. the two stores of a half-row in one statement AND the pointer steps/`if` not last). This is the C-shape question the
  next pass has to answer; it is NOT a flag/pragma/compiler-build question (mpv_mcy 4p stays 136w under every candidate — untested
  per-unit because no candidate unsplit the probe; steps 2/3 of the flag plan therefore did not run).
- adx_sje (14/17 -> 15/17): `adxsje_output_header` 2 -> 0w with `if (sje->key == 0) { v8 = 0; } else { v8 = 8; }` in place
  of the ternary (still the branchless `cntlzw/extrwi/neg/andc`, but the `li r5, 1` argument now sits one slot earlier = the
  target; the pass-14b "post-RA tie" had a source form after all — an `if/else` that MWCC if-converts keeps different
  instruction ids from the `?:`). `adxsje_encode_data` 68 -> 67w: `sji` as a helper local of the inlined read_pcm
  (`SJ *sji = sje->sji;` inside adxsje_read_pcm, the parameter dropped) colours r24 below cnt r25 = target (a caller own
  local ranks straight after sje: r28); the caller's `n` is still an own local (r28 now, target r20 below the loop-B IV temps);
  `n` declared last/first, a `prd->iir` local in loop D (target loads iir right after prd; ours after the scale copies) 68w.
  `adxsje_write_end_code` 2w stays: put16 site 1's `lha v` is scheduled before `lwz ck.data` in ours; `(Sint16)*(Uint16 *)`,
  `(Sint16)(Sint32)`, index forms, `-x * -1`, `Uint16` copies (lhz) all keep it or add a word. `#pragma scheduling off` is not
  a lever for a one-slot swap inside an otherwise identically scheduled 0x230 function (not tried on purpose).
- Applied this pass (all plain C except the two tags in sfd_mps): sfd_mps DecodeOneUnit 144 -> 13w (M1 pin `asm { mr r31, err;
  mr ret, r31 }` after the CopyPketData call) / ExecServerSub 59 -> 16w (helper split + M3 `#pragma dont_inline` around
  SFMPS_ExecServer); cftfx StaticV 60 -> 36w; adx_sje output_header 2 -> 0w, encode_data 68 -> 67w. Flags unchanged (nothing
  IDENTICAL); objects.py untouched; every applied step rebuilt through the locked ninja and judged with bytecmp
  (sfd_mps 22/26, cftfx 3/6, adx_sje 15/17). Scratch harness ~/.cache/cri29 removed at the end of the pass.

### CRI mwsfdcre pass 6 (CreateSfd 126 -> 115w, pure C: MallocFrmTbl's height/width declared BEFORE nfrm; the dead `b` and CalcWorkSfd 4w open; 8/10, not flipped; 2026-09-12)
Harness ~/.cache/cri_mws6/ (deleted at the end): `gen.py NAME BODY` (base.c with the IsUseAdxt body replaced), `try.sh NAME`
(variant.sh + differing lines), `probe/pr.sh`/`pr2.sh NAME 'case-4 body' [decl]` (a 20-line TU with an inlined IsUse switch, case 4 last/FIRST,
ra.py dump -> frontend-00/01 statement list + final PCode in one line), `rasum2.py` (rasum for pass 2).
- **Residue (1) closed (126 -> 115w, the 11 words were block 2 of the inlined MallocFrmTbl).** Read off the pass-2 dump: nfrm @755 = r106,
  height @752 = r109, width @751 = r110, ret @756 = r105, fsize r107, i r108 — the inlined helper's locals take ids in DECLARATION order
  (parameter/first declared = lowest), and the MACRO's block locals (`MWSFCRE_CALC_FRMSIZ` declares `height; width;` in an inner block)
  come AFTER the helper's own locals. height (30 neighbours) and nfrm (74) are both L2 and colour by descending id: height r109 first -> r19,
  nfrm -> r20 (target nfrm r19 / height r20). Fix: expand the size macro by hand inside mwsfcre_MallocFrmTbl and declare `ret, height,
  width, nfrm, fsize, i` (height/width before nfrm) — the PCode/statement order is unchanged, only the ids move. Pass 5's "block-2 colours
  follow the PCode first-appearance order" was wrong: it is declaration order, the macro's block locals just came last.

### CRI pass 34 (adx_dcd5 Ste4AsSte 118 -> 115w APPLIED, pure C, the target's colouring reproduced in the model except one node; Ste4AsMono 180w / adx_baif AIFF_GetInfo 170w read, unchanged; nothing flipped; 2026-09-12)
Harness ~/.cache/cri34/ (deleted): try.sh (variant.sh wrapper), probe.py / probe2.py NAME 'old=>new'.. (edit one function of a
scratch copy, ra.py dump + bytecmp + side-by-side), explore.py (chaitin.py graph edits: drop a ghost, move a vid, add an edge, clone a node;
scored against the target's registers), levels.py (the degree of every surviving node after each simplification scan = the level structure).
Tree edit: src/lib/adx_dcd5.c ADX_DecodeSte4AsSte body + comment only (locked ninja, bytecmp 2/4, 115w, sizes equal). objects.py untouched.

**Applied (Ste4AsSte 118 -> 115w, pass-30's three Mono4 facts carried over):** own locals declared `l2, r2, r1, l1, i, d, dr, Sint16 sc_l,
Sint16 sc_r, const Sint32 *qtbl, s, t, nblk, key, j, q_l, q_r`; `sc_l = ((s ^ key) & 0x1FFF) + 1; key = sadd + key * smul; *scl = key;
*scl = *scl & 0x7FFF;` (the Sint16 own local IS the extsh, the key redefinition blocks the hoist substitution); `q_l = qtbl[d & 0xF];
outl[0] = l2; q_r = qtbl[dr & 0xF]; outr[0] = t;` (table values as the last-declared own locals defined before the stores, `q_l * sc_l`);
`r2 = t` stays at the END of the body (moved next to t's clamp the copy is coalesced: t and r2 share r27, -4 bytes).

**How the Ste4AsSte target colours (read off the bytes, then replayed in chaitin.py on the v1 dump, `--check` IDENTICAL on ours):**
- Level structure of ours: scan 1 removes the 72 short temps; survivors then have degrees 25..35 (q_r 25, q_l 26, t 27, d 28, dr 29, r2 29,
  sc_l/sc_r 30, c-ext 30/31, i 31, l1/r1/l2/nblk/sadd 32, qtbl 33, smul 34, scl 35; the two `>>4` temps 26 after losing 3 — 29 total, so they
  survive scan 1 only because none of their L1 neighbours has a lower id); scan 2 removes everything but sadd/smul/scl/nblk, scan 3 those.
  No spill pick. Ours-Mono has 7 levels with nblk a spill PICK (cost 17 = the cheapest cost/degree) — the target's Mono has nblk r18 and smul
  r19 = nblk picked first, then smul (33/93 = 0.355 beats sadd's 33/91): the target's Mono graph is stuck after nblk's pick where ours frees
  four nodes.
- explore.py on v1: (1) dropping the nfrm ghost (r33 -> r4) makes both `>>4` temps 28 -> L1 and puts c2e/c1e in r10/r9 (the target);
  (2) moving the qtbl address to the own-local position (`const Sint32 *qtbl = AdxQtbl` with two uses is still substituted by the frontend;
  the address is a backend temp r76 coloured FIRST in L2) gives l2 r31, r2 r30, r1 r29, l1 r28, i r27, d r26, dr r25, sc_l r24, sc_r r23,
  qtbl r22 = the target; (3) an r2-t edge keeps t off r30. Left: the model hands t r21 / nblk r20, the target has an extra node X on r21
  before t (r20) and nblk (r19). X needs a new register (adjacent to d, dr, sc_l, sc_r, qtbl, i, l1, r1, r2, l2 = live inside the inner
  body before the idx computation), must be adjacent to t (or t takes r21) but to none of the target's r21 temps (s1, key2b, d>>4, c1r1,
  idx_l, c1l2, c1t); q_l/q_r reuse d/dr's r26/r25 only if coloured before r21 is handed. Those constraints leave X live only inside t's
  clamp: no C value. So an assumption is wrong — most likely the ghost set (the target may keep param copies ours propagates away, or lack
  the nfrm one) or the pre-RA order. Open; the 3 words gained are the sc/q/qtbl structure.
- **The nfrm ghost**: the `mr r33, r4` param copy survives to RA because its use `mr r3, r33` (`return nfrm`) blocks the backend's copy
  propagation (pass 05 propagates r35/r37/r38/r39 = histl/histr/c1/c2 into their loads/extsh but refuses a copy whose use is itself a
  copy; `return 0` removes the ghost — and r4). `nblk = nfrm >> 1`, `ret = nfrm` (substituted), a K&R definition: no change. The
  ghost is a neighbour of every loop value (+1 degree everywhere).
- Negative (do not retry): reversing the addition `((c1*l1 + c2*l2) >> 12) + (d >> 4) * sc_l` gives IDENTICAL PCode (the frontend
  canonicalises the operand order); K&R-style definition = identical graph; the Ste restructure applied to Ste4AsMono (with `c1 * r2` after
  `r2 = t` as the target's `mullw r20, r9, r31` shows) is 226w (worse than 180w) — Mono needs its own reading (t is short-lived there,
  r2 = t right after the clamp, `m` in r21/r20).

**adx_baif AIFF_GetInfo 170w (unchanged) — what the header shape says:** target: ckid = own local r27 (`mr r27, r30; rlwimi r27, r31, 24,
0, 7`), cksz = own local r28 (`mr r28, r12; rlwimi`), SWAP32(cksz) = a temp r12 computed in the header block, `end = p + (r12 - 4)` in the
loop preheader (`subi; add`), type = a temp r10 (no mr, substituted into `subis r10, r10, 0x4646` in the block after the FORM check). Ours:
ckid substituted (temp r10), cksz own local r30 (`mr r30, r28`), the swap = the @temp web r27, `end` = `add r10, r27, r8; subi` (reassociated
because the swapped web is an own local), type = own local r10 (`mr r10, r12`). The AST (frontend-01) shows the substitution is decided per
web: ckid's single use is in the same block -> substituted; type's use is behind the FORM `if` (a label between) -> kept. In the target
ckid is kept and type substituted, i.e. the roles are swapped: something keeps the header ckid (then the FORM `if` has no loads and type
can cross it). Probes without effect (170w each, identical objects): a dead `ckid = 0` init; `p = buf; ckid = LE32(p); cksz = LE32(p+4);
p += 12;` and `buf += 8` between the reads (the frontend copy-propagates `p = buf` / range-splits the stepped parameter, so the
redefinition blocker of SWAR pass 5 does not fire on straight-line code); type read through `ofst` (190w). `ckid = LE32(buf + 8)` reused
for the type check moves the loads into block 2 (149w, size equal) — the target's type loads are in the header block. Open: the statement
that keeps the header ckid a variable (a store between its def and the FORM check? the vendor's LE32 macro reading through a local
pointer that is then stepped inside a loop?).

### CRI pass 36 (sfd_mps 22/26 -> 24/26: CopyPrvate 60 -> 0 and CopyPketData 157 -> 0 in pure C; adx_sje encode_data 67 -> 39w; cftfx 3/6 unchanged; nothing flipped; 2026-09-12)
Harness ~/.cache/cri36/ (deleted): mk.py/mkp.py/mks.py/mkc.py block-replace variant runners over tools/research/kit/variant.sh, ghost.py/ghost2.py = chaitin.py as a library with injected nodes/edges, ra_* dumps.
- sfd_mps CopyPrvate 60 -> 0 (pure C): (1) CopyUoch reads `ch.sj` directly (frontend CSE temp, lowest vid -> coloured last, r23) instead of a `sj` local; fn1/fn2/obj stay locals. (2) CopyPrvate has an own `ret` (`if (bufout3 == 8) ret = 1; else ret = CopyUoch(..)` -> `li r24,1; mr r3,r24`); hoisting the `== 8` test into a helper `sfmps_CopyUo(sfd, chno, data, len)` called from both paths also OK. (3) THE residue (chaitin.py: one added edge len~len_a reproduces all 9 target colours): the header copy's arguments are the caller's own variables redefined before the call: `data -= 0x12; len += 0x12; CopyUo(sfd, 0, data, len);` -- a multi-def `len` is not propagated into the inlined param copy, `len` stays live past the `addi` (no in-place r28), len_a takes new r24, then data_b/ret reuse r24 and both sj CSE temps take r23; also gives the target's `mr r5, r24; cmpw r3, r5` (the kept helper-param copy sunk below the GetNumData call) and the 0x80 frame. Expression arguments (`data - 0x12, len + 0x12`), wrapper helpers (2-level: wrong frame order), `n = len` user copies, reversed compares: no.
- sfd_mps CopyPketData 157 -> 0 (pure C): (1) the CopyPketFn call passes `idx` (hd.raw[MPS_PKT_IDX]) as the stream-id argument, not `stmid` (semantic fix; type/idx/pts then load into r5/r4/r7:r8 right after the header block like the target); (2) the sj path stores its own `res` local (`*result = res`), the function `ret` stays 0 (removes the `li ret,0` after the store and un-merges `li r0,0` (stores) from `li r29,0` (ret)); (3) pass-12 shape: `void *obj` parameter + `SFD sfd = obj;` declared right after `wk` (sfd = own-local vid between wk and ret -> r30); (4) `outobj` declared BEFORE `fn` (obj coloured first takes sfd's dead r30, fn then wk's dead r31; the reverse gives fn r30 / obj a new r25 and shifts the params).
- adx_sje encode_data 67 -> 39w (pure C, applied): `n = (sje->blksmpl < sje->total - sje->nsmpl) ? sje->blksmpl : sje->total - sje->nsmpl;` after the bufs[] stores (the `?:` makes n's second def a frontend temp created after the bufs stores: n r20 below the loop temps = target; the if/else form kept n an own local r28); `iir = prd->iir` own local in loop D (the target hoists the iir load right after prd). Helper forms for the min (@ret) 62w, memset loop as a helper 64w, no `sjo` local 103w, separate ch/i counters or cnt/n declaration order: no change. Left 39w: magic `lis r3` vs `lis r4` + `lwz r30, 0xc(r29)` vs `(r3)` (ours propagates the obj->sje copy into the sjo load, the target's lis clobbers r3 first), cnt r25 / &sje->sji r24 swapped, memset-loop temps (target bufs-ptr r21, n*2 r22, ch r19; ours r19/r21/r22), loop-D ch r25 vs r19, prd/iir r6/r5 swapped.
- adx_sje write_end_code 2w: unchanged. put16 site 1: target `lwz r6, 0x1c(r1); mr r3, r29; lha r0` vs ours `mr r3; lwz r6; lha` — a post-RA dual-issue tie (lha cannot pair with the lwz, the ALU slot goes to `mr`; the pick order between lwz and mr inside cycle 0 differs). The GC/2.6 debugger build keeps the pre-RA order `lha; mr; lwz` in its final dump, so the 2.7 post-RA scheduler cannot be read there. Tried: `Sint16 *p` local, `>=` polarity (33w), early return, Sint16 v / -0x7FFF (17w), `#pragma scheduling off` (57w), n after the check (18w), `(Uint16)n`, adxsje_put(&v,2) (33w) — all no.
- cftfx UserTable 135w unchanged (box overrun, nothing applied). Read off the target: per block `subi w4 = ywidth-4` IN the loop, two kept copies `mr r7,w4; mr r4,w4`, each row's `addi y,4; add p,y,w4` literal (no folding), the 4th `ywidth-4` recomputed (`subi r26`), then `subf r4, r0(ywidth*4 hoisted), r26; addi y, r4, 4`; the packed word goes through an own local (`slwi r28; mr r25, r28; rlwimi r25` = pass-5 "copy into an own local never coalesces"), the row temporaries colour r25-r29 (all of r0,r3-r12 are taken by the invariants/pointers first = the invariants are level 2 in the target, level 1 in ours), frame stmw r25 (ours r30/r31). Probes: `w4` own local at the block top -> hoisted to the preheader and propagated (c1; also with a dead pre-loop def c2/c6/c7, chain `w1 = w2 = w3 = ..` k1/k3/k4, `w2 = w1` copies k2, three separate `wN = ywidth - 4` k5/k6: all substituted+hoisted); w4 USED after the inner loop (c8: `y += w4 + ..`) keeps the def in the loop and the adds literal (`subi` in-loop, `addi; add y,y,w4` x4, one register) but costs code; per-row `w4 = ywidth - 4; y += w4;` (k8/k11, 4-5 defs) = pass-5 web rule confirmed on a scalar: the LAST TWO webs are sunk (the 4th is recomputed at its use `add; subi` exactly like the target's `subi r26`), the earlier ones survive but are hoisted into ONE preheader `subi` (target: in-loop + 2 copies). Row helper returning `y + 4` (h1/h2), t-macro with own-local words (t1, m3-m6: the `u` word's loads move above the first store like the target, webs still sunk): 135w. Open: what keeps the three surviving `ywidth - 4` webs inside the loop as def + 2 copies.
- cftfx Argb420 38w unchanged: the target's `lwz r5 (pln.y); lwz r4 (pln.cb); subf r3 (half); mr r9, r4 (cb); mr r6, r5 (y); add r5, r5, r3 (a from the load temp)` = kept copies from the load temps into y/cb while the temps stay live for half/a. `pln.y`/`pln.cb` written directly in the half/a expressions (a1-a5: CSE'd into one load, but y takes the load and `add a, y, half` reads y), `Uint8 *py/pcb` user copies (a6/a7): all 39w — the frontend forwards the copy (uses of the temp after `y = temp` read y). Not found: what keeps the temp and y distinct.
- cftfx StaticV 36w: not touched this pass (2.6 debugger's RA order differs from the 2.7 output here too: w webs r30 in the dump, r31 in the object).
  Addendum: `#pragma opt_loop_invariants off` around UserTable does not keep `w4 = ywidth - 4` in the loop (p1/p2: the subi still lands before the loop) — the single-def own local is placed at the preheader by the frontend's propagation, not by the invariant pass.
- sfd_mps ExecServerSub 16w unchanged: ret r25 / len r26. Dump: ret's class = {@774 ret (r47), @796 AddRead's `ret` local (r43, leader = the LOWER vid)} vs len @775 r46 -> the class colours after len. Depth-2 clone locals (AddRead @796/@797) get LOWER vids than the depth-1 helper's (@770-@775) here. `void *obj` + `SFD sfd = obj` in ExecServerSub: no change (sfd already L3). `len` as the CSE temp `inf.ck1.len` with GetRead(sfd, &inf): 82w (the inf aggregate moves into the loop's frame).
  UserTable addendum: an inlined row helper with a `step` parameter (`return y + 4 + step`, called with `ywidth - 4`; s1/s2) folds even further (0x204). DecodeOneUnit: `unit = sfd->prm.unit` own local (single def d1-d3: 13w unchanged, substituted; multi-def d4/d5: 35w).
- Tree state at the end: sfd_mps 24/26 (DecodeOneUnit 13w with the pass-29 M1 pin, ExecServerSub 16w with the M3 pragma), cftfx 3/6 (unchanged), adx_sje 15/17 (encode_data 39w). Nothing IDENTICAL -> objects.py untouched, no flip. `ninja` prints "premature end of file; recovering" on every locked build (a damaged .ninja_log/.ninja_deps from an earlier unlocked run; not caused here).

### CRI pass 39 (adx_dcd5 Ste4AsSte 115w / Ste4AsMono 180w, adx_baif AIFF_GetInfo 170w: all read, none changed; the r21 owner of Ste4AsSte identified as `s` in level 2; no tree edit, no flip; 2026-09-12)
Harness ~/.cache/cri39/ (deleted): try.sh (variant.sh wrapper), mk.py (literal `OLD=>NEW` edits of a scratch copy), model.py
(chaitin.py replay of the Ste4AsSte dump with pass-34's edits: nfrm ghost dropped, qtbl at the own-local vid, r2-t edge), ra.py dumps
ra_b0 (AIFF_GetInfo), ra_ste, ra_mono. Tree untouched (src/lib/adx_dcd5.c, src/lib/adx_baif.c, objects.py as found).

**Ste4AsSte 115w — the pre-RA order is NOT the problem, `s` is the r21 node:**
- Pre-RA order verified (backend-12-after-scheduling of ours vs the target's final bytes): B8 `lbz d; mullw c1*l1; lbz dr; addi src;
  extsb; extsb; srawi d>>4; mullw c2*l2; add; mullw; srawi; add`, B20 `rlwinm idx_l; sth l2; lwzx q_l; mullw c1*l2; rlwinm idx_r;
  sth t; lwzx q_r; mullw c2*l1; add; mullw q_l*sc_l; srawi; add`, B30 `sth l1; mr r2,t; sth r1; addi; addi; bdnz` = the target's
  order instruction for instruction (ours differs only after the POST-RA scheduler: `mullw` hoisted above `lbz d`, `mullw c2*l2`
  above `srawi d>>4`, register-dependent). Pre-RA also carries dead `extsh` temps for every `Sint16` store (r109, r112, r123-r128) and
  dead `rlwinm d & 0xF` (r107, r110); they have 0 neighbours in the graph and do not extend `d`/`dr`.
- The model (pass-34 state: ghost dropped, qtbl own-local vid, r2-t edge) colours L2 in the order @63 r10, @64 r9, l2 r31, r2 r30, r1
  r29, l1 r28, i r27, d r26, dr r25, sc_l r24, sc_r r23, qtbl r22, **t r21, nblk r20**, q_l r26, q_r r25; the target has s r21, t r20,
  nblk r19. `s` (first web, `lha r21, 0(r3)` .. `addi r21` in place, `extsh r24, r21`) has vid r49 = exactly between qtbl (r50 position)
  and t (r48), so **X = s: if s is in level 2 it is coloured right after qtbl and before t**. Ours has s at 25 total / 23 at removal
  (level 1, coloured after the temps, r18). Raising s's degree by 6 in the model puts it in L2 but it takes r23 (sc_r's register,
  not a neighbour of s in ours: sc_r is defined after s dies); for the target's r21 s must ALSO interfere with sc_r r23, sc_l r24,
  dr r25, d r26 — inner-loop values that are dead at the scale read in every C spelling. Same dead end as pass 34 from the other
  side: the target's `s` web is longer or its graph has ~4 more inner-body values live across the outer body top. Not found.
- Target consistency checks with the lowest-free rule: `lis r20, AdxQtbl@ha` in the prologue = r19 (nblk) live, r20 (t) free; the
  second scale `lha r23, 0x12(r3)` takes sc_r's r23 = r19..r22 blocked for it (nblk, t?, X, qtbl live) — t adjacent to the second
  `s` web means t live across the outer body in the target's graph (not in ours). idx_l r21 (not d's r26) with q_l r26: d dead at idx_l,
  idx_l coloured after X with r21 the lowest free handed-out register.
- Hard pins (locals `r1`/`r2` must be renamed `rr1`/`rr2` first: "ambiguous use of local variable(r2) and assembler register"; the
  pinned local must be `register`, form `asm { mr r20, t; mr t, r20 }`): t r20 alone 70w, t r20 + nblk r19 59w (t lands in r21, nblk
  in r17, frame 0x40 -> 0x50: the pins add stack slots and do NOT bind — sadd/smul/scl move to r22/r0/r12, qtbl to r12), + s r21 87w,
  nblk r19 alone 114w, s r21 alone 75w. Not applied (frame size changes; a pin that does not bind is not a lever here).

**Ste4AsMono 180w (read only):** ours has ONE spill pick, nblk (17/34 = 0.5), after which four backend temps drop to < 29 and free
the graph: r106/r110 (`srawi t,2` / `rlwinm` of the `*7/10` division), r118 (`lwzx AdxQtbl[d & 0xF]` — a backend temp in Mono, not
an own local), r120 (`mullw c1*l2`), degrees 29/29/31/30. The target's second pick smul (33/93) needs all four to stay >= 29 after
nblk's removal, i.e. one more neighbour each (a value live across the whole inner body that ours lacks) — the same missing node as
Ste's. The Ste restructure (own-local q) is 226w here (pass 34); not retried.

**AIFF_GetInfo 170w (read, three mechanisms measured, none closed):**
- **`end = p + (x - 4)` is reassociated by the CODEGEN, not the frontend**: frontend-01 keeps `EADD(p, EADD(@229, -4))`, backend-00
  already emits `add end, x, p; addi end, end, -4`. Every spelling gives `add; subi` (135-169w): `sz` own local (`Uint32`/`Sint32`),
  `&p[sz-4]`, `(Uint8*)((Uint32)p + (sz-4))`, `(sz-4) + p`, `sz - 4 + p`, `sz -= 4; end = p + sz` (the range-split web is sunk into the
  use), `p + (Sint32)(sz-4)`. The target's `subi r10, r12, 4; add r10, r8, r10` needs `x - 4` as a value the codegen does not see through
  (a kept web with a blocker, or a non-EADD node); the swap in r12 across both `if`s with no `mr` = an @temp web (second web of a
  variable), not an own local (`sz` own local: `mr r11, r29; rlwimi r11`).
- **The 8-byte size gap = the two `clrlslwi r12, r12, 16, 8` at the COMM `*nch`/`*bps` reads**: `(p[1] & 0xFFFF) << 8 | p[0]` with p[0]
  inserted as a byte (`rlwimi 0, 24, 31`), p[1] treated as a 16-bit value of unknown range. Not reproduced by any spelling of a Uint8
  load: `(Uint16)(p)[1]`, `((Uint16)(p)[1] & 0xFFFF)`, `(Uint16)((Uint32)(p)[1])`, `(Uint16)((p)[1] << 8)`, `((p)[1] << 8) & 0xFFFF00`,
  `(Uint16)(p[1] + 0)`, a `Uint16 hi = p[1]` local (all folded: the compiler knows an lbz is 8 bits); `(Uint16)` at the macro ROOT
  gives `clrlwi 16` after the or (m1: 98w but +4 bytes and the loop's ckid grows a `mr`); a Uint16 helper for the byte or for the
  whole LE16 gives `clrlwi 16` after (127w). exp/mant in the target have NO mask (`rlwimi 8,16,23` then SWAP16 then `clrlwi 16`),
  so the mask is not a property of the macro alone (or the rlwinm-chain peephole removes it under SWAP16).
- **Header shape**: an inlined `static Uint32 aiff_LE32(Uint8 *p)` makes cksz and type mr-less (their @ret copies coalesce; 133w,
  -4 bytes) but never keeps ckid (still substituted into the FORM compare) and gives the swap web an `mr`; struct copy `AIFF_HDR h =
  *(AIFF_HDR *)buf` 159w at size 0x280 (lwz/stw copy); `switch (ckid) { case FORM: }` 156w; `do {} while (0)` around the header, an
  `aiff_IsId(ckid, FORM)` helper: identical objects. The target's F/S/SW/A roles (F = own local with `mr`, S = own local with `mr`,
  SW = @temp web r12, A = @temp web r10 live across the FORM `if`) need A to be a LATER web of a variable first defined in the header
  and F kept despite a same-block single use; no C arrangement found in which A's loads stay in the header block (the `ckid`
  reuse puts them in block 2, pass 34).

### CRI pass 37 (mpv_umc OneReadMb 48w / mps_lib MPS_Create 2w / adx_tsvr nlp_trap_entry 2w / sfh_main SFH_AnlyElemSmpHz 6w: four fresh angles, all negative; no source edit, nothing flipped; 2026-09-12)
Harness ~/.cache/cri37/ (deleted): try.sh / mk.py (substring variants over tools/research/kit/variant.sh, objects compared by
md5), sfhv.py (SmpHz body variants), ch.py (chaitin.py as a library with edges removed/added), ra.py dumps of the four functions.
No tree file was edited; the four functions keep the pass-31 forms. Sanity: GC/2.6 (the debugger's compiler) compiled with the unit's
flags gives the same 48w object for mpv_umc as GC/2.7 -- the ra.py dumps are faithful to production (fdiff: target LEFT, ours RIGHT).
- **mpv_umc `mpvumc_OneReadMb` 48w: the RA side is now quantified.** vx (r40) has 33 neighbours; chaitin.py on the dump says removing
  the vx-fn_y edge (or vx-fn_y + vx-chx) does NOT move vx off r6 -- vx is coloured 12th (degree-at-removal 11, level 2) as long as its
  TOTAL degree is >= 29, and at that position only r0/r3/r4/r5 are blocked, so r6. The target's vx r25 / cvx r28 (= ypos's colour) /
  vy r11 / cvy r7 are LATE colours: those nodes were simplified in the first scan (degree < 29) and coloured after every r0-r12
  temporary, taking the lowest free handed-out callee-saved. So vx needs degree <= 28 = five fewer neighbours, i.e. its last use
  (`rlwinm yhx`) issued before fn_c's row/idx temps (r92/r94), cvx>>1 / cvy>>1 (r84/r85), the mullw (r86), the ofs[1] reload (r66) and
  the fn_y load -- exactly the pass-31 pre-RA schedule question. Measured: stride+dst stores moved above the vector loads (60w): the loads
  sink to the block end, `rlwinm yhx` precedes them, vx's degree drops to 29 -- ONE short of the threshold, still r6. Negative
  variants (all 48w, same object md5 e035213f unless noted): `Sint32 iy = yhx; fn_y = tbl_y[vy & 1][iy]` (propagated; identical
  object), `tbl_y[vy & 1][yhx]` (identical), yhx through a `static Sint32 mpvumc_Hx(Sint32 v)` helper (48w, different md5: the
  @ret copy is coalesced, no priority gain), the helper doing the `& mcflag` too (73w: the two-definition web is lost), the two
  src/src2 stores through an inlined `mpvumc_SetSrc(mc, src, pitch, hx)` helper (48w), `*(volatile Sint32 *)&yhx` as the index (82w).
  Fact for the next attempt: the pre-RA scheduler issues `lwzx fn_y` and `rlwinm yhx` in the SAME cycle (load listed first); the load's
  edge over the rlwinm is the WAR chain to the four `mc->` stores; `and yhx` is live-out only (height 1). The lever must give the first
  yhx definition an in-block consumer chain of height >= the store chain, or take 5 values out of vx's range; neither exists in C
  without changing the target's instruction set. Left 48w (the M-class lever `#pragma scheduling off` is 87w, pins 74w: not applied).
- **mps_lib `MPS_Create` 2w: it is a WITHIN-CYCLE order of the post-RA scheduler, not a tie by id.** Pre-post-RA (backend-16) B10 is
  `stw r6,0; li r5,0; li r4,-1; addi r0,r3,@l; stw r5,4; mr r3,r31; stw r5,8 ..`; the post-RA list is a 2-issue cycle model (cycle 1:
  `li r6; lis r3`, cycle 2: `stw r6; li r5`, cycle 3: the two fillers `addi r0` + `li r4`, cycle 4: `stw r5,4; mr r3,r31`, then one
  store per cycle). Ours orders the cycle-3 pair addi-first BOTH when the addi has a lower instruction id (the fn-pointer address
  formed in an own local `Sint32 (*fn)()` assigned before the -1 stores: 49w, `addi r4 / li r0` colours swap but addi still first) and
  when it has the higher id (base) -- so the addi's post-RA priority is strictly higher: its WAR successor `mr r3, r31` (the @ret copy,
  r3 rewritten) plus the late `stw r0, 0xd4`; the `li r4` only has its stores. Not moved (2w, identical md5 772cc2dd): `~0`,
  `(Sint64)-1`, `-1LL`, `Sint32 m1 = -1` assigned after `x10 = 2`, `return (MPS)(void *)mps`, `fn` assigned after `packhd.rsv` /
  before `pkethd.pts`; the chained `scr = rsv = mux_rate = -1` is 65w (+4 bytes); moving `packhd.scr` above `x10` / `x10` above
  `errcode` only permutes the stores (4-5w). The store stream is issued in source order (a store chain or in-order LSU), so the
  `li r4`'s consumer position cannot be advanced without a visible store permutation. To flip the pair the addi must lose its WAR edge
  (the lis would have to land outside r3, i.e. r3 busy after the memset call -- no C value lives there) or the `li -1` must gain a
  successor above the stores; neither has a C spelling here. Left 2w.
- **adx_tsvr `adxt_nlp_trap_entry` 2w, graph read:** the `lha ofst` temp (r56) has neighbours r1, r3, ofst2v, ofst1, sji, sjd, p,
  n1 and the n2 ghost (r54 -> r3): lowest free = r0. The target's r4 needs an r0-coloured neighbour in the join block B16
  (`lha; cmpi n1; lha ofst2v; add`), i.e. a value defined in BOTH arms (`li n2,-1` / the ScanInfoCode result copy) or in B16 before the
  add -- the then-arm defines nothing but n2 (r3) and B16 defines only ofst2v (r26, callee-saved: it lives across the later calls), and
  no value can be r0 across the else-arm's call. The only r0-capable candidate is a coalesced ghost, and the ghosts here all target
  r3/r4 (argument moves, @ret). So the target's extra r0 neighbour has no C source in this control shape; the 20-spelling record of
  passes 11-23 stands. Left 2w (the 3-pin form moves the 2w to `subi r5` / `mr r3, sji`, pass 19b).
- **sfh_main `SFH_AnlyElemSmpHz` 6w (M4): the fold is neither register- nor type-dependent, and the target's r6 word is explained.**
  (1) The word's register is NOT the fold's condition: a hard `asm { lwz r6, 0x1c(e); mr w, r6 }` pin gives `stwbrx r6, r0, r5` (22w:
  the pin level-shifts every temp of the search); r7 17w, r4 6w. (2) Store type/spelling is irrelevant: `*(Uint32 *)val`, `val[0]`,
  `(Sint32)` cast, a `Sint32 x` local, `(x & 0xFF) << 24` / `(x & 0xFF00) << 8` / plain `x >> 24` term spellings -- all 6w, same
  object. (3) `+`/`^` instead of `|`: no pre-RA merge at all (rlwinm x4 + add/xor x3, 8w); the balanced tree `((a|b) | (c|d))` gives
  TWO rlwimi accumulators joined by an `or` that the post-RA peephole does NOT merge (4w, `rlwinm r4; srwi r0; rlwimi r4; rlwimi
  r0; or r0, r4, r0`) -- and there the loaded word takes r6 (r4 blocked by the second accumulator), i.e. the target's r6 word is a
  second partial result live at RA time, consistent with the pass-11 reading that the original merged the chain after allocation.
  Mixed `(a|b) + (c|d)` 4w, `0xFF00FF00/0x00FF00FF` halves + rotate 10-13w, byte assembly from `aud_smphz[0..3]` 17w. (4) The
  target's own evidence that the fold is order-blind in ours but not in the original: adx_bwav's `*sfreq = SWAP32(*(Uint32 *)(p +
  0xC))` (macro order `(x>>24)&FF | (x>>8)&FF00 | (x<<8)&FF0000 | x<<24`) IS `lwz r6; stwbrx r6, r0, r27` in the target, while the
  Sofdec order `(x<<24) | (x<<8)&FF0000 | (x>>8)&FF00 | (x>>24)&FF` gives the target's `rlwinm 8,8,15 / rlwimi 24,0,7 / rlwimi 24,16,23
  / rlwimi 8,24,31 / stw` chain and ours folds it. With peephole ON our compiler folds every linear four-part chain into `stwbrx`
  whatever precedes or follows it (the pre-RA scheduler cannot place an independent instruction between the last rlwimi and the stw:
  `li r3, 1` is a cycle-1 filler); the only non-folding forms are non-linear chains, which are not the target's instructions. Lever
  left: `#pragma peephole off` for this ONE function (M4) with the swap as SFH_SWAP32_STORE and the element search re-spelled to the
  folded displacements -- pass 23 measured 10w / 38-41w for those, so the C form (6w) stays.

### CRI pass 35 (cri_cvfs cvFsOpen 18 -> 0w, unit 11 -> 12/13, cvFsGetFileSize 32 -> 14w; sfd_adxt ExecServerSub 88 -> 59w at target size; sfd_tst SFTST_Calc 79w read, unchanged; no flip; pure C, no pins; 2026-09-12)
Harness ~/.cache/cri35/ (deleted at the end): `try.sh <unit> <variant.c> <FUNC> [-d]` over variant.sh, ra.py dumps,
`exp_tst.py` (chaitin.py replays with edited `g.adj`/`g.order`). fdiff.py columns: LEFT = target, RIGHT = ours (pass 32 read them
the other way round for SFTST_Calc: the target colours mt.hi FIRST, ours LAST).
- **The colour rule, once more, with the arithmetic that matters:** "lowest-NUMBERED free register among those handed out" means
  a node coloured after r31..r20 are all handed out takes r20 (not r23) when r20 is free. So "X needs an rN-coloured neighbour"
  is only true for registers BELOW rN; a node that gets a HIGH callee-saved register late (SFTST_Calc mt.hi r30, cvFsOpen dev2 r22)
  was coloured EARLY (few registers handed out) or with everything below blocked. Read the target's colouring ORDER from the
  registers by replaying "lowest free" over the handed-out set, then compare with chaitin.py's order on our dump.
- **cri_cvfs cvFsOpen 18 -> 0 (pure C): the second and third device searches are INDEX loops.** `cvfs_SearchDev(tbl, name)`:
  `len = strlen(name); for (i = 0; i < CVFS_MAX_DEV; i++) if (strncmp(name, tbl[i].name, len) == 0) return cvfs_tbl[i].vtbl;`.
  The stepping pointer is then the frontend's strength-reduced IV @temp, copied from `tbl` in the loop PREHEADER (after the
  `bl strlen` = the target's `mr r20, r26` position) with a LATE id created in program order between the first and the second
  strlen @ret copies (target order i1 r22 > dev1 r21 > len1 r20 > dev2 r20 > len2 r21; the pointer local `dev = tbl` was an early
  clone local next to `i`, coloured before the lens -> r22 NEW). The third copy coalesces with the dying `tbl` (in place, `addi r4,
  r26, 4`). The first search (cvfs_FindDev via cvfs_WantsDevForm) keeps the POINTER form (target dev1 r21 = an early clone local).
  cvFsGetFileSize follows: 32 -> 14w.
- **cvFsGetFileSize 14w = one missing never-removed neighbour of fname AND pdev.** chaitin.py on the new dump: fname L3 (27 at
  removal) r29, tbl (@1414, ResolveDev local) L2 r28, pdev (own local, 28 at removal) L2 r27; target fname r29, pdev r28, tbl r27.
  Model: `+1` ghost on {fname, pdev} (or on every node) gives exactly the target (fname L4, pdev L3, tbl L2); `+1` on pdev alone
  puts pdev above fname (r29). cvFsOpen has the neighbours already (dir/rw live across everything) and is identical, so the
  missing node is a coalesced copy (ghost) or an L2+ value live across the ResolveDev region that GetFileSize alone lacks — or one
  specific to cvFsGetFileSize: in the model a +1 ghost on the ResolveDev region also moves cvFsOpen's fname r28 -> r26 / pdev
  r27 -> r28 / len3 r26 -> r27, so it is NOT inside cvfs_ResolveDev. Rejected: `if (pdev ==
  NULL)` instead of `if (dev == NULL)` (pdev L3 above fname: r29/r28, 15w; the target's `addic. r0, r1, 0x134` tests the array
  address), dropping `pdev` (103w, frame changes), `asm { la r28, dev(r1); mr pdev, r28 }` pin (20w: shifts fname/rodata, the
  level-shifter effect of pass 18b).
- **sfd_adxt ExecServerSub 88 -> 59 (pure C, size 0x428 kept): every block is its own depth-1 helper.** `sfadxt_Transfer` (as
  before), `sfadxt_PrepOut(sfd)` {bufin, bufout}, `sfadxt_CheckStat(sfd, len)` {wk, adxt, adxterr, stat, tst}, `sfadxt_AnalyAhdr(sfd)`
  {ahdr, adxt}, `sfadxt_UpdateSvrFreq(sfd)` {wk, adxt, freq}, `sfadxt_WriteTotSmpl(sfd)`. Breadth-first cloning gives each later
  block's locals LOWER ids than the previous block's, and every depth-1 local a higher id than the depth-2
  `sfadxt_UpdateFlowCnt` local `wk` (coloured last of all -> r24 = the lowest free after wk r24 died; as a helper called from
  Transfer it was coloured before the own locals and took a NEW r28). Inside a helper the colour order is the REVERSE
  declaration order (first declared = lowest id = coloured last): CheckStat `wk, adxt, adxterr, stat, tst` -> stat r28 (backend
  temp anyway), tst r27, adxterr r26, adxt r25, wk r24; AnalyAhdr `ahdr, adxt` -> IsDecoded adxt r24, ahdr r25; UpdateSvrFreq `wk,
  adxt` -> adxt r24, wk r25 — all the target's. One helper for PrepOut+CheckStat is NOT inlined (-inline auto size limit;
  strip_unused then fails on the surviving static) — split them. Every remaining word (59) is sfd r30 / err r31 (target sfd r31,
  err r30, len r29): L2 = {@464 err (Transfer's `err` local coalesced with the @ret, 21 at removal), sfd (27), len (23)}, coloured
  by id. Model: either sfd +2 never-removed neighbours (L3) or the caller's `err` as a NODE (the coalesced chain ranks by its
  lowest-id member: err r3x < sfd) gives the target. The caller's `err = sfadxt_Transfer(sfd, &len)` is propagated into the @ret
  (no node). `Transfer(sfd, &len, &err)` writing `*err` on each path (void) makes err an own local with the 3 defs (`mr r30, r3`,
  `li r30, 0`, `mr r30, r3`) and colours sfd r31 / err r30 / len r29 = the target (40w, -4 bytes): the only residue is
  `if (*err != 0) return;` -> `bne` where the target's value form keeps `beq L; b end` (the deleted coalesced `@ret = err` copy
  separates the branch from the goto, pass 13b). `*err = ret; return;` in that arm keeps the `mr` (+4 bytes); returning `*err` with
  the value unused deletes the dead copies (= void); `ret = Transfer(sfd, &len, &err)` / `err = Transfer(.., &err)` interfere with
  err (101w). `err = 0` before the call (dead, removed), `register err`, `Uint32 err` / Uint32-returning Transfer (49w, the @ret
  bounces through r3) do nothing. Not closed.
- **sfd_tst SFTST_Calc 79w read (three regions, all ranking; nothing applied):** with the correct column reading the target's
  sprintf-block order is mt.hi (r21, FIRST) > out.hi r22 > out.lo r23 > MulDiv r24 > mt.lo r25 > mt_max.hi r27 > mt_max.lo r28 >
  hlp.hi r29 > hlp.lo r30; ours colours MulDiv (backend temp r403, L2, first) > out.hi > out.lo > mt.lo > mt_max > hlp (L2 by id)
  > mt.hi (L1, 27 total: it dies at the first dead `subfe`, out.hi at the second) -> r30. The @temps are the ECOMMA chain
  @171 out (r60/61), @172 mt (r58/59), @173 mt_max, @174 hlp (arg 2's `hlp.cnt / hlp.unit` is NOT replaced by @174; args 3-6 are);
  a pair's hi has the higher id. Model: mt.hi +2 neighbours puts it in L2 (6 of the 9 colours right) but it is then coloured after
  out (lower id); the level route needs out.hi/out.lo in L3 (+9 each, 20 at removal) — impossible from the same code. So the
  target's ids differ: MulDiv must rank between out.lo and mt.lo (a temp created between @171 and @172, not a backend temp) and
  mt.hi above out.hi (not the hi half of the mt pair). Also: ave/tol region target tol.hi r23 > ave.hi r22 > tol.lo r21 (ours ave.hi
  L3 r23, tol.hi r22, tol.lo r21 -> tol.hi needs +4 to reach L3 above ave.hi); abs region target adiff.hi (@119) r23 before
  diff.lo (backend r226) r25. The debug block as a static helper changes nothing (79w). No pin exists for @temps; not closed.

### CRI SWAR kernels pass 8: the 16x16 4p body cannot be ONE block from C (initial count >= 110 for its 122 final instructions); the target is two blocks split after `d[1]`/pixel 9 — the first block needs > 100 initial instructions there; nothing applied, nothing flipped (mpv_mcy 4p 136w, H2/V2 225w, mpv_mc 4p 72w / V2 73w / H2 436w unchanged; 2026-09-12)
Harness ~/.cache/cri_swar8/ (kept until the next 4p pass; delete after): `try.sh <abs variant.c> [FUNC] [unit]` (ra.py dump into
`ra_<name>` + `bs.py` block sizes + variant.sh words; `LINES=1` adds the per-source-line instruction census of the biggest block),
`bs.py DIR [--lines]`, `ast.py DIR` (one line per frontend-01 statement: line, kind, AST size, assigned var), `gen2.py NAME order=..
rot=.. pack=.. sum=..` (event-ordered 4p bodies: L<k> loads, P<k> sums, S<w> stores, DC, ST, IF; `unit=mc` for the 8x8), `mk.py NAME
BODYFILE`, `dis.sh <abs variant.c> FUNC [unit]` (dtk mnemonic listing of the variant), `mcmp.py A.lst B.lst [N] [-v]` (register-masked
schedule compare), `tgt16.lst`/`tgt8.lst` (target listings). ra.py runs must be SEQUENTIAL (parallel mwcc-debugger runs corrupt each
other's dumps: fixed gdb port) and the out dir removed first (try.sh does).

**Count arithmetic (settles pass 7's open question):** backend-00 of ours is 148 instructions, peephole-forward (backend-01) RAISES it to
160 (`or` -> `mr` + `rlwimi`: +3 per pack), RA coalesces the `mr`s (158 -> 134), the loop transform adds 2, prologue 5; nothing else is
created. So every final instruction of the body except the 12 pack `or`s is 1:1 with an initial PCode: dcbt 1 + 34 lbz + 64 add/addi + 16
pack (28 initial as `rlwinm/or`, 27 per store statement as `__rlwimi` intrinsics = g2, worse) + 4 stw + 3 steps = 122 final, >= 110 initial
even with a perfect 4-instruction pack. The check (`pcodeCount > 100` before every ST_EXPRESSION/IF/GOTO/RETURN/SWITCH) therefore fires
before the `if (i == 7)` in every one-block C form. Probed and rejected as check-evaders (probe TU = 49-51 x `x = x*3+1` + the construct +
two stores, all SPLIT): inline `asm { add .. }` instructions COUNT (pa1: 98 + 10 asm = B2 108 | 3), `__rlwimi`/`__dcbt`/`(Uint8)` casts count,
`d[1] = (d[0] = X, Y);` and `(void)(d[0] = X, d[1] = Y);` ARE one statement (pa5/pa6: 3 instructions, no check between the stores) but
the following statement is checked as usual, statements inside `switch` case bodies / `if` arms / `do {} while (0)` are checked (pc1-pc6;
the switch's dispatch blocks are created BEFORE the body block yet pclastblock is still the body). The frontend unrolls small constant-trip
inner loops (u4: `for (k = 0; k < 4; k++)` with a 6-instruction body -> 4 copies as ordinary statements in the outer body, each checked);
bodies of ~42 instructions (u3, one word per iteration) and ~60 (u1/u2, one half-row) are NOT unrolled -> a real inner loop remains. An
`if (i == 7) d += 16;` is an ECONDASS ST_EXPRESSION in the AST (checked like any statement); `d += (i == 7) ? 18 : 2` is 181w (join copy).
Unreferenced labels, `goto L; L:`, `if (i == i) goto L;`, `switch (0) { case 0: break; }` are all folded by the frontend (no block boundary).

**The target's body is TWO blocks, split after `stw d[1]` (or after the pixel-10 loads), not one:** the constraints "a load never passes a
store within a block" + "blocks never merge" give: pixel-9 loads (`lbz 9(r5)/9(r6)`) and all four adds of p8 (`addi r31,r7,2` between the
two stw, `add r31,r9,r31` right after `stw 4(r3)`) are in the block of `stw d[0]/d[1]`; pixel 10..16 loads, p10..p15 (their last adds sit
after `rlwinm r26,r30` = the d[16] pack start), d[16], d[17], the steps and the `cmpwi/bne` are in one block. A boundary between P9 and L11
is excluded (`lbz 0xb(r5)` precedes p9's second add). With the boundary after `d[1]` both blocks fit the rule: block 1 = DC + 20 lbz + 9 sums
+ 2 packs + 2 stw, block 2 = 14 lbz + 7 sums + 2 packs + 2 stw + 3 steps (67 + ...) <= 100 before the `if`. Evidence from the schedule: a
variant with the target's raw order (pixel 9 loaded and `p8 = a8 + a0 + b8 + b0 + 2` KEPT before `d[0]` — kept because `a8` is redefined
by the pixel-10 load before its use, pass 5's redefinition blocker — a1.body) and a forced boundary after `d[1]` (`if (stride < 0) return;`,
a2) starts the body exactly like the target (`lbz b1, a2, b0, a1; add a2+b1; lbz b2; add a1+b0; lbz a3` — ours unsplit is address order
`b0, a1, b1, a2`), diverging at the 9th instruction (target hoists `lbz b5`); the isolated block is what changes the scheduler's picks.
Consequence: pass 6/7's "one block of 124" reading was the pass-7 inference from the d[16]/d[17] interleave only; the first-half/second-half
interleave never existed.

**What the split needs and what is still open:** the check fires before the first second-half statement only if block 1 holds > 100
INITIAL instructions there, i.e. the vendor's first half emitted >= 30 instructions more than ours (ours: 1 + 20 + 36 + 28 + 2 = 87 with
7-instruction packs incl. p8; the split must fall exactly after `d[1]` or after the pixel-10 loads, so 77 <= count(after d[0]) <= 100 <
count(after d[1]) with S1 = 24, or the same with L10 = 2 last). No natural C spelling found that emits >= 30 later-deleted instructions
in that half: inlined static helpers for the sums/packs are substituted by the frontend (f1-f4: zero copies, identical object), `Uint8`
pixel locals add no conversions (g4, 135w), chained shifts `((p << 11) << 11)` are constant-folded (g1), `(p >> 2) << 24` packs emit
srwi+slwi that the peephole merges only partly (d1: 208w, +0x30), identity `__rlwinm(x,0,0,31)` pairs merge to one survivor (pb1), `x + 0`
/ `x | 0` / `x * 1` / block-scoped copies emit nothing (pb3-pb5). Candidate mechanisms not yet probed: a copy-emitting form (the frontend's
`@N = (int)stride` copy for the dcbt is the only `mr` in ours), the vendor's second half written as ONE statement (a comma/nested-assignment
store pair: pa5 shows it is not split), an `asm` statement holding the second half (ST_ASM is never checked and its instructions are scheduled
with the block; excluded by the no-asm-body rule unless it is a single instruction).

**Other facts read this pass:** (1) the frontend re-associates the pack `A | B | C | D` into `D | (C | (A | B))` (AST of line 52), so the
codegen emits p3, p2, p0, p1 and the or->rlwimi merge (earlier operand fused, later = base) gives the target chain `rlwinm p1; rlwimi p0,
p2, p3`; any other operand order changes the chain (O0132: 22,30,6; O3210: base 6) — the pack macro order is ours. (2) The sums
`a0 + a1 + b0 + b1 + 2` become `a0 + (((a1 + b0) + b1) + 2)` (target association) in every spelling tried. (3) Forwarding: with separate
variables (g9: a0..a16, p0..p15) every sum is forwarded into its store except p15, whose RHS took the single-use pixel-16 loads and is then
blocked by the d[16] store; with the reused a0..a8/p0..p7 set the first web of the reused `p7` is kept (base) — a reused variable's first
web survives when a store lies between its def and use, a single-def variable's does not. (4) The 8x8 4p target's schedule is NOT the
sliding raw order: forms with the stores right after their 4 sums (m1-m4, 78-91w) put `stw d[0]` before the pixel-5 loads; the target has
all 18 loads before `stw d[0]`. The 8x8 flat form (ours, 72w) is the raw order; its residue is the same scheduler-pick question as the
16x16 first block (target issues p1's chain before p0's: `add a2+b1` first; ours `add a1+b0`).

### CRI mwsfdcre pass 7 (no source edits; CreateSfd 115w + CalcWorkSfd 4w unchanged; 8/10, not flipped; both mechanisms narrowed, neither closed; 2026-09-12)
Harness ~/.cache/cri_mws7 (deleted): `try.sh NAME 'helper text' [callargs]` (whole-unit variant with the IsUseAdxt body and its two
call sites replaced), `pr.sh NAME 'case-4 body' [decls] [params] [args]` + `show.sh` (25-line TU through ra.py: frontend-01 statements + final
PCode blocks; env RET/FALSE/DFLT rewrite the return/FALSE arm/default arm), `cw.sh NAME 'tail'` (CalcWorkSfd's last two statements replaced).
- **Dead `b` (CreateSfd 115w), new negatives, all 115w:** unreachable statements are deleted by the frontend before layout, whatever they are —
  `case 4: break; break;`, `break; return TRUE;`, `break; return FALSE;`, `break; mode = 0; break;`, a bare `;`, an empty `{ }`, `{ break; }`,
  a user label `lbl: break;`, `goto end; break;` with `end:` before `return TRUE` (the case label is forwarded to the goto's target), a def whose
  only use is unreachable (`x = mode + 1; break; return x;` / `mode = x;` / `x = x + 1`, `if (0) return x` after the switch) — frontend-01 deletes
  the unreachable use first and then the def. A nested `switch (mode) { case 4: break; default: break; }` is deleted whole; `switch (mode) { case 4:
  return 1; default: break; }` keeps its tree, the backend CSE deletes the inner `cmpi` but keeps the `bt` (`bt cr0,2,X; b T` + `X: li 1; b`).
  `return TRUE; break;` = 124w (`li; b` kept). The struct-parameter forms: `IsUseAdxt(MWSFD_CRPRM *cprm)` with `switch (cprm->mode)` RELOADS
  `lwz r0, 0(r16)` at both sites (the frontend does not CSE `cprm->mode` across the intervening calls; 12w, and the dead `b` still missing) —
  so the helper's operand is the caller's `mode` r21 and a `cprm->mode` re-read in the body cannot be the deleted statement (p1 `if (cprm->mode
  == 4) return TRUE;` 133w: load+cmpi CSE'd, `bne; li; b` kept).
- **Dead `b`, positive mechanism read in the ra.py TU (form A):** `Bool ret = TRUE; switch (mode) { case 4: ret = TRUE; break; case MPV: case
  VONLY: ret = FALSE; break; default: break; } return ret;` gives EXACTLY the target block layout — tree, `B7: b T` (the emptied case-4 block laid
  out before F), `F: li 0; b end`, T empty — because the frontend keeps `ret = TRUE` (ret has a live use) and the backend CSE deletes the `li` as
  redundant with the dominating init; but the init is `li r0,1` scheduled into the tree's first block and T is empty (the target has no `li`
  before the tree and `li r0,1` at T). With `default: ret = TRUE; break;` added (init dead on every path) the FRONTEND deletes the init and the
  case-4 `li r0,1; b` is kept (form B/C = 115w shape). Hence the constraint: the deleted case-4 def must be redundant with a dominating def that
  costs nothing at BOTH inline sites (a coalesced copy or a value already in a register there), and `ret`'s fall-through value must still be
  produced at T. No caller value fits: the only constants in callee-saved registers before site 1 / site 2 are conditional (`li r26,-1` in the
  MallocFrmTbl loop, `li r20,0; li r19,0` in the else arm). Still open; not a compiler difference (the shape is one statement away).
- **CalcWorkSfd 4w, substitution rule pinned down:** the frontend forward-substitutes the single-use chain of `size` (`size = @1019 + adxwk;
  size += 0x4000; += 0x700; += 0x100` -> merged `+ 0x4800`) into ANY rvalue read of `size` in the next statement — `return sibsiz + size`,
  `return size + sibsiz`, `sibsiz += size`, `sibsiz += (Uint32)size`, `Uint32 total = sibsiz; total += size`, `return sibsiz + (size +=
  FNAME)`, `Uint32 size`/`register Sint32 size` declarations, `size2 = size; sibsiz += size2` — all give one EADD tree that the BACKEND
  reassociates (`addi r0, r25, 0x4800` onto adxwk, 7-14w). Only an lvalue use (`size += sibsiz`, the current form) is not substituted, and it
  fixes the operand order `add size, size, sib` (ours) — the target's `add r3, r29, r3` is `sibsiz += size` / `sib + size` with `size`
  NOT substituted. Proof: `sibsiz += size; if (size == 0) return 0; return sibsiz;` (size two-use) gives `add r29, r29, r3` AND the unhoisted
  epilogue (`lmw; lwz r0; mtlr` after the add) — both residue words fall together, at the price of the guard's own 4 instructions. So the
  original read `size` twice (or had a frontend-kept boundary between the chain and the sib add); the second read compiles to nothing in the
  target. Not found: an inlined helper with one return leaves no label (CalcYccSize has none in frontend-02); the if/else join L@1004 is
  before the chain (rfb/tab come from the arms), so it cannot separate the chain from the add. The peephole that hoists the epilogue fires
  on a single-predecessor return block whose first instruction is the `lmw`; the target's return block starts with the `add` or has two preds.
- Flags: `lib/mwsfdcre.c` stays False (8/10); objects.py untouched; no source edits this pass.

### CRI pass 38 (sfd_cre AnalyMpv 15w / sfx_zmv MakeOrgZ32TblByCCIR 74w unchanged; the post-RA reschedule is gated by a per-block "scheduled" bit that the IV-increment sink of the post-schedule peephole clears; nothing flipped; 2026-09-12)
Harness ~/.cache/cri38/ (deleted: gen.py/genz.py variant generators over tools/research/kit/variant.sh, cmpblk.py block-vs-target aligner, flags.py = per-block flag word / sunk? / pre==post? summary of an ra.py dump dir; 15 min to rebuild from this text).
- **MWCC block flag 0x8 = "scheduled".** In the ra.py dumps the `:{xxxx}` word before each block carries it: the pre-RA scheduler
  (backend-17 in CCIR, -11 in AnalyMpv) sets 0x8 on every block; every later pass that rewrites a block CLEARS it (backend-18
  peephole-forward on the loop blocks, backend-22 peephole on B7/B11); **the post-RA scheduler (backend-23) reschedules ONLY blocks
  whose 0x8 is clear** (CCIR: B3/B15/B23/B43/B47 keep 0x8 through pass 22 and are untouched by pass 23; every 0x...4 block is re-run).
- **backend-18 (peephole-forward after the pre-RA schedule) sinks the loop COUNTER increment (`addi i,i,K`, the IV of the loop's exit
  test) to the block end, in every loop block** (CCIR B8/B12/B29/B37/B40/B52, MakeCnvZTbl B13/B25/B32/B43). That clears 0x8, so
  every loop body is rescheduled post-RA. In every block but CCIR B8 the post-RA re-run reproduces the pre-RA order (17 == 23 by
  opcode), so the target cannot show whether it happened there. In B8 it does not: post-RA the eight `stw r0, hi(r1)` 0x4330 stores
  are free of the `stw lo` of the same 8-byte temp (distinct offsets), pre-RA the same-object accesses are ordered, so the post-RA
  run hoists them (and the lfd/fctiwz chain) — the 74 words.
- **CCIR B8 in the target = ours' backend-17 order exactly (91 instructions, opcode-for-opcode, the `addi r3,r3,8` at slot 27 where
  the scheduler put it) = never sunk, never rescheduled.** So the vendor's B8 was not dirtied after the pre-RA schedule: the sink
  did not fire on it. Not the reason: block size (a 92-instruction B8 is still sunk), `volatile` store (same object), `#pragma
  scheduling off/603/750`, `peephole off` (118w), every `opt_*` pragma (see z2.py), `i += 1` / `i = i + 1` / do-while spellings
  (identical object), an inlined `sfxzmv_inc(i)` increment or a `?:` increment (kills the 8x unroll, 145-147w).
- sfd_cre `sfcre_AnalyMpv` 15w: the pre-RA scheduler is a 2-wide top-down list scheduler, one load per cycle; heights lbz 2 /
  int 1 / cmpi 2 (through the bt); an int op with an in-block successor (`addi ofs+1` -> `subf size`) always beats a zero-successor
  load, whatever the raw order (30 more raw orders / spellings of `size -= ofs + 1`, `data++`, `ofs++`, `size = size - 1 - ofs`,
  `(Uint32)ofs + 1`, byte loads interleaved with the size statement: all keep `rlwinm; addi; cmpi`). The target has `rlwinm; cmpi;
  lbz b6; addi` i.e. its addi lost cycle 2 to a load: its `ofs + 1` had no in-block successor or was not ready at cycle 2 — the
  `subf size` was not a DAG successor of it there. `x++`/`x--` statements are deferred by the frontend to the block end (line of the
  following `if`), so `data++` is a sinker after `size -=` in ours (target: `addi data` at slot 9, `subf size` at 11 = the reverse).
  `register` locals for asm operands (`asm { addi t, ofs, 1 }`, 12w) and `#pragma scheduling off` (41w) do not close it. Not closed.
### CRI pass 40 (sfd_adxt Matching 27 -> 28/28, cri_cvfs Matching 12 -> 13/13, both with one M1 hard pin; sfd_tst SFTST_Calc 79w read again, unchanged; 2026-09-12)
Harness ~/.cache/cri40/ (`try.sh <unit> <X.patch> <FUNC>` = a replace-list patch of the tree source through variant.sh; deleted at the end).
- **sfadxt_ExecServerSub 59 -> 0: `asm { mr r31, obj; mr sfd, r31 }` (register obj/sfd).** The pass-35 helper split had every register but sfd r30 / err r31 (target sfd r31, err r30, len r29). The hard pin
  takes r31 out of every other node's range, so err (Transfer's coalesced @ret chain, coloured before sfd by id) falls to the NEW r30 and len to r29; no other register moves, size 0x428 kept. 111 OK, flipped.
- **cvFsGetFileSize 14 -> 0: a CODELESS level-shifter pin, `asm { mr r11, pdev; mr pdev, r11 }` after `pdev = dev` (register pdev).** The dump: after iteration 1 only errfn-base (32), rodata-base (33),
  fname 27, tbl (@1414) 26, pdev 28 remain; iteration 2 removes pdev (id 33) then tbl (id 45) -> tbl coloured first (r28). One extra never-removed neighbour on pdev makes it 29 in iteration 2: tbl goes
  alone, pdev in iteration 3 (28), fname in iteration 4 (27) -> fname r29 > pdev r28 > tbl r27 = the target. A physical register IS such a neighbour, but a callee-saved pin also RESERVES the register
  (`asm { mr r27, tbl; mr tbl, r27 }` in ResolveDev: pdev r28 right, tbl r26 and every lower local -1, `stmw r22`; pass 35's `la r28` pin: the loads went through r28 and the rodata base fell to r27) and
  a pin of the top node (fname r29) puts the colour count at 28 and cascades the rodata base to a spill removal (r27). Pinning to a VOLATILE register nobody in the function uses (r11, r8 both give 0w)
  adds the neighbour and reserves nothing the function wanted: the two `mr`s are deleted (size equal), pdev keeps the direct `0x134(r1)` loads because the pin is AFTER the propagated copy. Same effect
  from inside ResolveDev with r11, but the caller-side form leaves cvFsOpen's inlined copy untouched. 111 OK, flipped.
- **tcDataExport 193 -> 62 words (applied, size exact): `register int found asm("r5") = 0;` in loop 5** (tag in the source). One pin
  settles the cascade closer 2 predicted: found no longer takes r7 first, loop-5 d -> r7, i -> r6 (`mr r6,r5` back), j (reg 97, now
  refs 52 len 254 pri 10236) falls below the loop-2 giv base (10326) -> giv base r4, j r12 everywhere, loops 1/3 identical. Pins that
  made it worse on top of it (do not retry): loop-5 `d` -> r7 (180w, size -4), `cd` -> r3 in loop 2 (192w, size +0x20), a `register
  u8* buf asm("r29")` copy of the parameter (105w, size -0x2c), j -> r12 (closer 2).
- **The 62 words left are one allocation fact: buf r29 / pTc-high r30 (ours the reverse) and the `d+1` PRE copies of loops 2 and 4
  in r30.** GORDER (V1): buf 167 pri 3105 pass1 -> r30 (first virgin callee-saved), pTc-high 179 (reg 358, REG_EQUIV-doubled len 54,
  calls 1) pass1 -> r29; loop-2 d+1 (reg 355, 2105) `[scan pass1 r3]` (r3 was in regs_someone_prefers, virgin in pass 1 -> taken
  BEFORE r31/r30), then cd (reg 397, refs 9 len 186, 1451) -> r29; loop-4 d+1 (reg 354, 4000) pass0 -> r7, tcCdat base (reg 268,
  1428) -> r5, pTc-high (reg 263, 416) -> r4. In the target both d+1 copies are r30 and buf r29, so at loop-4 d+1's turn (before buf)
  r0,r3-r12 AND r31 were all in used1 (hard_reg_conflicts, not smpref: find_reg pass 1 walks reg_alloc_order r0,r9,r11,r10,r8,r7,r6,
  r5,r4,r3,r31,r30 and takes the first non-conflicting one) - the target's loop 4 has r4 free in the final code, so the r4/r5/r7 holders
  that conflicted with d+1 are pseudos allocated BEFORE it whose registers were later reused; ours allocates the loop-4 tcCdat base and
  pTc-high after it (pri 1428/416 < 4000). Not found: what gave those two (or a third value) pri > 4000 in the target - a shorter
  live length (a body that reloads `pTc`/`tcCdat` per iteration?) or more refs. The downstream words (loop-5 pTc-high r31 vs r4,
  tcTypeTbl r3 vs r29, `mr r10,r25`/`mr r7,r28` order) follow from buf/pTc-high. Suids/insn numbers: `tools/research/kit/rtl.sh` `.lreg`
  of the tree source, loop 4 preheader insns 1239-1251 (regs 364 high, 263 pTc-high, 268 tcCdat, 366 = fp-buf).
- **tcSetBesideOffset 27 unchanged.** The giv pair n*12 (reg 207, refs 9 len 84) / n*4 (reg 209, refs 9 len 82) needs one more weighted
  ref on 207 (floor_log2 stays 3: 3*10/84 > 3*9/82); a codeless `asm("" : "=m"(c->pos[n]))` / `"=m"(c->at[n])` anchor in the body
  (before/after the stores or before `n++`) does NOT add a ref to the reduced giv: it becomes its own address giv and moves c/o to
  r28/r29 (38-74 words). No pin possible (loop.c pseudos). Left as is.
- Flags: t_camera/t_camera_data and t_esp/db_widget stay False in config/G4BE08/modules.py; `ninja -k 0` + `dtk shasum -c` 111 OK after
  the two edits. Scratch ~/.cache/tcam2 and ~/.cache/tcam3 deleted; the kit untouched.
- **The pin as a NEIGHBOUR lever (the dump of the cvfs variant):** the pinned value is coalesced with the asm's copies, which stay as ghosts aliased to the
  physical register (`r34..r39 -> r12 = @1420..@1425` in the GC/2.6 dump), i.e. never-removed neighbours of THAT value only (pdev 28 -> stays >= 29 through
  iteration 2; total degree unchanged). So `asm { mr rV, x; mr x, rV }` with rV a volatile register the function never uses = "+k never-removed neighbours on x,
  nothing else" (holds x one Chaitin iteration longer = coloured earlier); with a callee-saved rV it also reserves the register; on a parameter or a value defined
  by a call it becomes a real copy (SFTST_Calc `asm { mr r12, tol@hiword; .. }`: `mr r21, r12` emitted, 108w; the parameter pins lose the hoisted .bss base, 139w).
  `x@hiword` is accepted for a `register Sint64`. chaitin.py reads the pinned dump with 6 divergences (it does not model the alias ghosts); the numbers above are
  the compiler's.
- **SFTST_Calc 79w, the target's colouring order read off the registers (LEFT = target):** sprintf block: out.hi r22 > out.lo r23 > [MulDiv r24] > mt.lo r25 >
  mt_max.hi r27 > mt_max.lo r28 > hlp.hi r29 > hlp.lo r30, mt.hi r21 LAST (L1, 27/27 in ours too: it takes the lowest free, r21 in the target because r22..r30 are
  all held by the group, r30 in ours because the group started at r21). Ours: the same L2 group in the same id order (@171 out r60/61, @172 mt r58/59, @173, @174;
  hi = the higher id) but started at r21 and with MulDiv (backend `mr r403, r3`, 20/38) coloured at index 9 in L3 -> r23. One story covers all three regions:
  in the target MulDiv is coloured with r24 the lowest free handed-out register, i.e. BEFORE `ave` (own local r44/r45, 28/51 + 28/50, index 6/8 in ours, r23/r25)
  hands out r23; ave then lands r22 (hi) / r25 (lo) = the target's region 2, and tol (@123) r23/r21 after it. So `ave` must drop one level: it is removed in ours
  only after the est spill picks (r185 est.lo 35/136, r184 est.hi 32/139; trace: r185, r107, r184, r35, r34, r33, r32), so one of its L3 neighbours is missing in
  the target's graph (or MulDiv has one more). Not found: `Sint32 ave` (119w, size -0x30: 32-bit arithmetic), `Sint32 ave32` copy (79w, propagated), `aave` folded
  into the condition, `movave_2nd = movave_1st = ave`, ave declared first / last (all 79w: the rank is a level, not an id), `msec = UTY_MulDiv(..)` as a statement
  (85w: the CSE loads move below the call - the target evaluates the call inside the argument list), `sec`/`msec` statements (85w). Region 1 (diff lo/hi r25/r23 vs
  r23/r25, pass 14) unchanged. No codeless pin exists: every pinnable own local here is 64-bit or defined by a call. Not closed.

### CRI pass 44 (neighbour-pin lever applied: adx_dcd5 Ste4AsSte 115 -> 41w APPLIED; Ste4AsMono 180w unchanged; sfd_mps / adx_sje / cftfx / sfd_tst below; 2026-09-12)
Harness ~/.cache/cri44/ (mk.py literal `OLD=>NEW` edits, v.sh = mk + variant.sh one-liner, ra_* dumps; deleted at the end). Lever row added to the MWCC table of the Lever catalogue (the last row).
- **What the pin register does (measured on Ste4AsSte, one `asm { mr rV, s }` after the first scale read):** the pin register is RESERVED for the whole function
  (r11: smul r11 -> r0, sadd -> r20; r23: sc_r r23 -> r22; r18/r14: `stmw r18/r14`), the pinned value never takes it (pass 40's tbl/r27), and its ghosts also raise the
  degrees of other nodes (q6b dump vs base: qtbl 76 -> 80, i 73 -> 75, sc_l 65 -> 66, d 38 -> 39), so the levels of OTHER nodes move too. Every volatile register of
  Ste4AsSte is used in the target (sadd r0, smul r11, scl r12, c-ext r9/r10, params r3-r8), so the only free pin registers are the PARAMETER registers: r3/r4/r5/r7
  (src/nfrm/outl/outr, coalesced ghosts `r32->r3` ..) cost nothing but behave differently from r6/r8 (histl/histr: copy-propagated, no ghost; the register is live only at
  the entry loads and the exit stores). Numbers, s pinned: r3/r4/r5/r7 70w, r6/r8 57w (both positions, both webs = one node), r23 76w, r21 75w, r0/r12 112w, r2/r13 176w
  (+4 bytes: r2/r13 are not allocatable, the mr stays). A single `mr rV, x` (no copy back) is enough and gives fewer ghosts than the pair (qtbl: 41w vs 46w; two
  registers `mr r6, x; mr r8, x` 81w = too many).
- **Ste4AsSte 115 -> 41w APPLIED: `register const Sint32 *qtbl; qtbl = AdxQtbl; asm { mr r6, qtbl }` (tag M1 neighbour pin).** Colours now: c2e r10, c1e r9, l2 r31, r2 r30,
  r1 r29, l1 r28, i r27, d r26, dr r25, sc_l r24, sc_r r23, s r21 (in place), nblk r19 = the target; frame/stmw r19 equal, the `mr` deleted. Residue 41w = the L3 set:
  ours smul r0 / scl r11 / qtbl r12 with sadd L2 -> r22 and t r21; target sadd r0 / smul r11 / scl r12, qtbl L2 at the own-local vid (r50, between sc_r and s -> r22),
  t r20. qtbl is still the backend temp r76 (the asm read does not stop the frontend substitution; 80 total = L3). Adding an s pin on top (r6 56w, r8 81w), a t pin
  (41w, no change), sadd pin (58w): no. Next: a spelling that keeps `qtbl` an own-local web (multi-def or asm-defined `lis/addi` with the hi part in its own local) so the
  address node sits at vid r50 with < 29 neighbours at its iteration-2 turn, and sadd back in L3.
- **Ste4AsMono 180w unchanged.** Pins of the four >29 backend temps are impossible (no names); pins on the named whole-loop values: sc_l/sc_r inner-body 158/153w at
  the target size 0x2f0, r2 (inner body, r6/r8) 117w, t (before the inner loop, r6/r8) 116w, m 135w, l1/l2/r1/i/nblk/d/dr 206w (r6 variants) - but every 116/117w
  variant has `stwu -0x50; stmw r17` (one more callee-saved register + 0x10 frame): the ghosts raise the pressure at the spill picks instead of fixing the nblk/smul
  pick order. `register` alone on r2/t/m: 180w (no effect). Not applied.

### CRI SWAR kernels pass 9: the 16x16 4p block split IS reachable from C (masked byte loads + `(Uint32)` operand casts = 32 deleted initial instructions, B3 = 105 split exactly after `d[1]`, pre-schedule DAG clean); the first block's schedule is still 29-39/67 positions (pre-RA load order 14-16/20); the target's block 1 WAS post-RA rescheduled (the matching 1p kernel's loop blocks are); nothing applied, nothing flipped (mpv_mcy 4p 136w, H2/V2 225w, mpv_mc 4p 72w / V2 73w / H2 436w unchanged; 2026-09-12)
Harness ~/.cache/cri_swar9/ (KEPT for the next 4p pass; delete after): `gen9.py NAME pix=.. sum=.. pack=.. order=raw|a1
lorder=.. sorder=.. inter=1 ba=1` (4p body generator over base_mcy.c: every spelling below is an option), `run.sh NAME opts` (gen9 + ra.py
dump `ra_NAME` + `bs.py` backend-00 block sizes + words), `runb.sh` (run.sh + `blk.py` block-1 positional compare against tgt16.lst: 67
instructions from `dcbt` to `add r31` = the target's block 1), `pre.py DIR` (block sizes + dead defs in the dump right before the first
scheduling pass), `dumplst.py DIR [B] [pass]` (a dump block as a dtk-style listing), `loads.py LST..` (the 20-load order of block 1
vs the target), `search.py N seed fixed-opts` (random raw-order variants, 0.5 s each, no ra.py), `sched.py` (list-scheduler model:
does NOT reproduce ours, do not trust). The pass-8 harness was deleted. ra.py runs must stay sequential.
- **Initial-instruction costs, measured on backend-00 (B3 = the body block up to the split; base 111 with 87 before `d[16]`):**
  `a0 = s0[0] & 0xFF;` on a `Uint8` load = +1 `rlwinm 0,24,31` per load (c6: 18 in the first half), deleted by two EXTRA backend passes
  the mask brings into the pipeline ("constant-propagation" turns it into `mr`, "load-deletion"/copy-propagation remove it) — all before
  the first scheduling pass, DAG clean; `(Uint32)a1` on a `Uint32`/`int` pixel variable used in TWO sums = a frontend CSE @temp =
  1 `mr` per such variable (s3/c2/c4: 14 in the first half), deleted at backend-02 copy-propagation; the nested `@t = (Uint32)a1`
  assignment ANCHORS the sum statement (the adds are emitted at `p0 = ..`, not inside the pack statement) and a pixel whose only uses
  are the cast (a6/a7 in z1) gets its load forwarded into the first sum — raw order changes with it; `Uint8` pixel locals = +1 `rlwinm`
  (zero-extension at the use, CSE'd) per two-use pixel (c5: +14, final 135w); pack `(((p>>2)&0xFF)<<24)` (m8) = +3 per pack and
  `((p&0x3FC)<<22)` (mk) = +4 per pack, BUT their extra rlwinm survive as DEAD nodes into the scheduler (pre.py: 12-14 dead vs 6) — not
  clean; `(Uint16)`/`Uint8`-typed sums or averaged temporaries keep real masks (196-222w); `(a0+a1)+(b0+b1)+2` changes the association
  (134w); `x + 0`, `(Uint32)` on single-use operands, nest/cast on `Uint32` pixels used once: nothing.
- **The split:** `pix=s32m sum=cast order=a1` (z1: masked loads, casts on every sum operand, pixel 9 + `p8` before `d[0]` as in pass 8's
  a1 body) gives B3 = 105 with the boundary exactly after `d[1]` (count after `d[0]` = 97 <= 100 < 105), B4 = 81, and the block before
  scheduling is 79 = the target's 67 + 6 pack `mr` + 6 fused-operand dead `rlwinm`   (identical to what the target's build had). x9
  (z1 plus `pack=m8`) also splits there (111) but with 6 extra dead rlwinm; x8 (z1 plus `pack=mk`) splits after `d[0]`
  (101: one instruction too many). So the target's `> 100` in block 1 is one of these spellings (or an equivalent 28-34 deleted
  instructions); the vendor most likely wrote the byte loads with `& 0xFF` and/or the averages with explicit `(Uint32)` casts.
- **Schedule (open):** with the split right, ours matches the target's block 1 for the first 9-10 instructions and diverges at 10
  (target `lbz b5`; ours `lbz a4`/`add`), best 39/67 positions over 150 random raw orders (s2_131: `lorder=0,1,4,2,3,5,6,7,8`); the
  PRE-RA schedule (dumplst.py + loads.py) has the target's load order at 14/20 (z1) and 16/20 (r1, loads interleaved with the sums:
  `b1 a2 b0 a1 b2 a3 b5 [a6 b3] a4 b4 a5 b6 [a7 a0] b7 a8 b8 a9 b9`, target `b5 b3 a6 .. a0 a7`), the POST-RA reschedule then puts
  our loads back into address order (`b3 a4 b4 a5 b5 a6`) and moves `add(a1+b0)` before `lbz a3` (target too). **The target's block
  1 was post-RA rescheduled**: RA clears the block's "scheduled" flag 0x8 when it coalesces the pack `mr`s (z1: B3/B4 `200c -> 2004`
  at after-regalloc, B2 without copies keeps `000c`), and the byte-identical `MPVMC16_OneRef1p_TuneC` has its six loop blocks (4 `mr`
  each) cleared and REORDERED by backend-14 — so the vendor's pipeline did the same and the final order is pre-RA schedule -> colours
  -> post-RA schedule (register-dependent: the target's block 1 lives in r7-r12 + r21-r31, `stmw r21`; ours r0,r3-r7,r22-r29).
  The pre-RA scheduler is priority-driven (loads deferred behind taller ALU chains: `a0` at 28-37 in both; `rlwinm p1` at 22 in ours
  vs 38 in the target = ours ranks the pack base by the `mr`-lengthened chain), ties by raw order (load/sum statement order moves the
  picks: r4/r5 sum orders diverge at 3, `lorder=5,6,..` puts `lbz b5` at 10). sched.py (height/latency list model) does not
  reproduce even ours — read the algorithm before more permutations.
- **Peephole fusion rule corrected (pass 5 said "earlier-defined operand"):** `or d, a, b` fuses ONLY the FIRST operand `a` when it is
  an rlwinm (`mr d, b; rlwimi d, a_src`); a non-rlwinm first operand leaves the `or` even when `b` is an rlwinm (`w |= P2'` stays `or`,
  acc0/inpl2), and `x = P0' | x` forms are forwarded into one tree anyway (inpl5-7: base p0 chain, 138-151w). The `mr` per fused `or`
  is inherent (dest != b), the 1p kernel has them too; no in-place chain exists.
- **Transfer:** mpv_mc `MPVMC08_OneRef4p_TuneC` with masked loads / casts / `Uint8` pixels stays 72w but the object CHANGES (128-148
  listing lines: the deleted instructions shift the virtual ids -> colours -> post-RA order; 10-14/76 masked lines vs 13 base) — a
  colour-order lever for the 8x8 kernels, not explored; mpv_mcy V2 with `(Uint32)` casts on the word operands (x0 = .. or the AVG2V
  arguments): 225w unchanged. H2 (mpv_mcy/mpv_mc) not touched.
- Flags unchanged; objects.py untouched; no tree source edited; 111 not re-run (no build).

### CRI pass 42 (cftyp422_ppc 6 -> 7/8 identical, 105 -> 29w: Init 9 -> 0w pure C, Y84C44 96 -> 29w with the dcbz index as a register variable; .data/.bss pads are link alignment, not objects; not flipped; 2026-09-12)
Harness ~/.cache/cri42/ (tryfn.py = replace one function body in a copy of the tree source + variant.sh; model.py = chaitin.py
what-if driver) and the cri33 tools, both deleted at the end. Kit only. 111 OK after the pass (unit still False, objects.py untouched).
- **Init 9 -> 0w, pure C — a redefinition of a variable the RHS reads blocks the single-use substitution (pass 5's fourth blocker,
  applied deliberately).** The y product must be a variable node (vid below the two hoisted literal loads, which are BACKEND temps
  f32 = 1.164 then f33 = the 0x43300000 conversion magic; all of Init's FPR values were backend temps, coloured in descending
  vid, so the product temp f36 took f6 before them). `k = i - 16; v = 1.164f * (Float32)k; k = i - 128; *py++ = v; *pcb_g++ =
  -0.392f * (Float32)k; ...` keeps `v` (the RHS reads `k`, which is redefined before the store) with the literal order unchanged:
  v f8, magic f6, 1.164 f7 = the target. Negative: statement order (product first, store third — the target's schedule stores y
  third, but two stores between def and use do NOT block the substitution), `v = 1.164f; v = v * conv` (the constant becomes an
  in-loop `lfs`), a helper `ret` local (substituted, 9w unchanged), `i = i` between def and use (deleted).
- **Y84C44 96 -> 44w: the `li r0, 8/4` + `dcbz d, r0` asm pinned r0 over the WHOLE function.** A physical register named in an
  asm block is live from the first asm to the last one (pass 33's "K = 28, r0 excluded" model was describing exactly that); the
  target uses r0 for the loop-2 setup temps (`srwi r0, r7, 31`) between the two loops, so its index was an ordinary node. Form:
  `register Sint32 ofs;` declared FIRST (highest own-local vid; degree 84 -> spill-pick level -> coloured first, lowest free = r0),
  `ofs = 8;` / `ofs = 4;` where the `li r0` asms were, `asm { dcbz d, ofs }`, `asm { dcbt y0, ofs }`, `asm { dcbz c, ofs }` (the
  asm operand must be a `register` variable). Loop 2's body and the crv/cbv edge residue vanished with it (crp0 r3, crv r6, cbv
  r10, crp1 r9 = target). Negative: `__dcbz(d, 8)` / `__dcbt(y0, 8)` intrinsics — the literal is materialised at the intrinsic and
  the backend loop code motion hoists it ONE loop level only (inner-loop preheader inside the outer loop, `li r6, 8` at the
  outer body top; the target's `li r0, 8` is in the function prologue) and it is coloured after the unroll count; a two-def
  `Sint32 ofs = 8; .. ofs = 4;` with the intrinsics is constant-propagated into both loops (register or not); `*++d = w0` C
  stores fold the four increments into `stfd 8/0x10/0x18; stfdu 0x20` (add-propagation) — the four `stfdu` need the asm stores.
- **44 -> 40w: `y0++; y3++; y2++; y1++;`** — the three `addi rY, rY, 0x20` of the unrolled body follow the increment statement
  order (target y3, y2, y1); pure C.
- **40 -> 29w: loop-2 setup order `cnt, cskip, hblk, cw`** (was cnt, hblk, cskip, cw): the `ywidth/2` sign-fix `add` is CSE'd
  between `cnt = src->ywidth / 2 / 4` and `cskip = (src->cbwidth - src->ywidth / 2) / 4`; with cskip second the cbwidth load
  is created after the add and coloured r10 (add r9) = target. All 24 orders tried; `cw` before `hblk` reschedules (31w).
- **Residue 29w (two mechanisms, read off the dumps and the model):** (1) loop 1: target ywidth r9, yw3 r10, unroll-remainder
  copy r11 (`mr r11, r4; andi. r11, r11, 3`); ours copy r9, ywidth r10, yw3 r11 (+ the prologue/loop-2-setup reschedule that
  follows from the anti-dependences). chaitin.py replays ours exactly (K = 29 now) and moving the copy's vid (r155, a
  loop-transform backend temp) to `n`'s slot (between yw3 r66 and y3 r64) reproduces the target with nothing else changed; adding
  up to 8 edges to ywidth/yw3 does not. So the target's remainder count IS the own local `n` (or a node with n's vid). The
  transform always emits `mr t, n` (probe: `while (n-- > 0)`, `for (n = cnt; n > 0; n--)`, `for (n = 0; n < cnt; n++)`,
  `for (k = 0; k < n; k++)`, `while (n) {..; n--}`, guarded `do {} while (--n)` (not unrolled the same way), with or without a
  separate loop-2 counter) and pass-07 copy propagation rewrites it to `mr t, cnt` because `n = cnt` is a copy — `asm { mr n, cnt }`
  and `asm { mr r11, cnt; mr n, r11 }` are pcode `mr`s and are propagated the same way (the hard r11 then poisons y3 -> r20,
  129w); two reaching definitions of n (`n = cnt` before the outer loop and at the body end) block the propagation but cost an
  extra `mr` (85w). Not found: a source whose `n = cnt` survives as the count node. (2) loop-2 setup: target cw r9 / cw3 r10
  (`addze r9, r6; mulli r10, r9, 3`), ours cw r6 / cw3 r9: the target's `cw` has a neighbour coloured r6 (the height sign-fix temp
  or the `cw3 + cskip` add) that ours does not — cw live across one of them, i.e. a different cw/o1/o2/cw3 expression grouping.
- **`.data` 0x8/0x4 and `.bss` 0x1428/0x1424 are NOT missing objects:** the sym_map entries `lbl_8026F6A4` / `lbl_803097E4` are
  split-tool fill (original name "."), the next unit (cftfx) starts 8-aligned right after; bytecmp reports them as `pad` and
  they do not block IDENTICAL (verdict is .text-only). Do not add a static for them.
- Flags: `lib/cftyp422_ppc.c` stays False (7/8, 29w). Tree = src/lib/cftyp422_ppc.c (Init pure C; Y84C44 asm dcbz/dcbt/stfdu with
  the `ofs` register operand, commented at the function).

### CRI pass 41 (sfx_zmv Matching 8/8: MakeOrgZ32TblByCCIR 74 -> 0w, pure C; the post-schedule addi sink compares store data-register NUMBERS without the register class; sfd_cre AnalyMpv 15w not touched; 2026-09-12)
Harness ~/.cache/cri41/ (deleted; 15 min to rebuild): `flags.py DUMPDIR` = per block the `:{xxxx}` flag word at passes 17/18/19/22/23 +
the slots of every `addi rX,rX,K`; `try.sh NAME` = ra.py dump of `NAME.c` into out_NAME + flags + `variant.sh --no-diff`; `gen.py NAME` = base.c with the
CCIR definition replaced by bodies/NAME.txt; `opc.py [mnemonic..]` = PCode opcode indices read off the GC/2.6 mwcceppc.exe opcode table (VA 0x5C0FA8,
471 x 0x12, first dword = mnemonic string; B 0, BDNZ 0xb, ADDI 0x3f, RLWINM 0x67, MR 0x8b).

**Applied (pure C, zero code, tag M1 removed): one counter per loop (`Sint32 i, j, k, l; Uint32 *d;` — j loop 1, i the 1.164 loop, k loop 3,
`d = tbl; *d++ = ztbl[ytbl[l]]` loop 4).** Same bytes everywhere; the only thing that changed is the VIRTUAL register number of the 1.164 loop's
counter: it is now the own local `i` = r43 (nine own locals p/i(macro)/ztbl/ytbl/d/l/k/j/i, reverse declaration order, i declared first = highest), and
the sink stops at `stfd f43`.

**The sink, read off the binary (GC/2.6 mwcceppc.exe, peephole-forward per block = 0x501930, called from 0x500e80 for every block with >= 2 pcodes;
the `addi` case at 0x5025e0).** For a pcode P = `addi rX,rX,imm` (dst == src, arg2 immediate) it walks the following pcodes S of the block:
- `(S.flags & 4) && S.arg0.reg == X` -> **stop, P is left where it is** (flags 4 = store: arg0 is the DATA register). The compare is on the 16-bit
  register NUMBER at pcode+0x28 only, no kind check: a `stfd fN` with N == X stops the sink exactly like a `stw rX`. That is the vendor's B8: its
  counter had the number of one of the eight `stfd` data FPRs (37 + 6k: f37 f43 f49 f55 f61 f67 f73 f79), ours was r44 (base: 5 locals + 4 webs
  of the macro's `i` before the loop-2 web @352).
- `(S.flags & 6)` load/store with base `S.arg1 == X` and an immediate offset that still fits 16 bits after `+ imm` -> fold the imm into the offset
  and move P after S (delete P when S is a load INTO X: `addi X,X,K; lwz X,d(X)` -> `lwz X,d+K(X)`); another `addi rY,X,imm2` -> fold and move after.
- `(S.flags & 9)` (branch-class pcode, e.g. `bdnz`): if a non-addi pcode has been passed and P is not already adjacent -> move P to just before S.
- otherwise scan S's operands: a GPR operand `== X` with the read/write bits -> move P to just before S (the B52 case: `addi r46,8` lands before
  the next `add r48,r38,r46`); if S is the block's last pcode -> append P at the end.
So the sink's target is "just before the next reference, else the block end", the move dirties the block (0x8 cleared -> post-RA reschedule),
and the one no-move exit is the store-data-number match. Every "later read of i" hypothesis of pass 38 was a red herring: nothing reads i.

**Numbering facts used (the register of `addi rX,rX,8` in the pass-17 dump is the quick readout):** own locals r35.. in reverse declaration
order after the parameters (unused `sfxz` gets none); a variable's FIRST loop runs on the variable itself, later loops get range-split webs
numbered after all own locals (base: macro-`i` webs @353-@356 = r40-r43, then function-`i` loop-2/3/4 webs @352/@351/@350 = r44/r45/r46). Counted
probes (all 74w, code unchanged): loop 3 or loop 4 on `j` -> r45; loop 1 on `j` (loop 2 becomes i's first loop -> own local) -> r40; loops 1+3
on j,k -> r41; 1+3+4 on j,k,l -> r42; + `Uint32 *d` pointer form of loop 4 -> r43 IDENTICAL (x2, applied); alternatively the macro inlined with
its loops on two counters m/n -> r43 IDENTICAL (x1). Locals the frontend propagates away get NO number (`Uint8 white = 0xFF`, `Uint8 *y0 = ytbl`);
`lim = 0xEB` as the bound survives but makes the trip count variable (110w). A fully unrolled loop's counter still gets a number.
Negative (all keep the sink, most identical 74w): `j = i` in the body / after the loop, loop 3 continuing on i (98w), inner-block `k = i`,
`i = j` step (no unroll, 145w), `j = ++i` / `i++, j = i` for-increments (dead j), do-while with `j = i` after `i++` (191w), hand-written 8-step
main loop with `p = ytbl + i` (B7 = the same 91-instruction block, still sunk; the hand remainder is unrolled again, 179w), pointer IVs (131/198w),
declaration order of ytbl/ztbl/i (74/184w), removing the `ztbl` local (r43, sink stopped, 266w elsewhere).
**Lever (MWCC table row to add): a loop body whose final order equals ours' pre-RA schedule but not ours' post-RA one = the vendor's IV update
was never sunk = its counter's virtual number equalled a store data register number later in the block; change the count of own locals /
webs before the counter (extra loop counters, a pointer form of another loop) until the pass-17 `addi rX,rX,K` register hits one of the
block's `stfd`/`stw`/`stb` data numbers after the addi's slot.**
- sfd_cre `sfcre_AnalyMpv` 15w not touched (time). Flags: `lib/sfx_zmv.c` True (CRI pass 41 block in objects.py), 111 OK.

### CRI pass 43 (sfd_mps ExecServerSub 16 -> 0 and adx_sje encode_data 39 -> 0 in pure C: sfd_mps 25/26, adx_sje 16/17; DecodeOneUnit 13w, write_end_code 2w, cftfx 3/6 unchanged; nothing flipped, 111 OK; 2026-09-12)
Harness ~/.cache/cri43/ (deleted: `try.sh <unit> <variant.c> [FUNC]` over tools/research/kit/variant.sh, `model.py DUMP names [+|-a:b ..]` = chaitin.py with edited edges, `snap.py`/`order.py` = removal-rule experiments, ra.py dumps ra_*).
- **sfmps_ExecServerSub 16 -> 0 (pure C): `len` is an OWN local of ExecServerSub passed as `Sint32 *len` into the inlined
  sfmps_ExecServerLoop (which passes it on to GetRead and reads `*len` for Decode).** The dump had ret's class = {@774 ret r47,
  AddRead's `ret` @796 r43 (leader)} coloured after len @775 r46. Own locals of the real function sit at r33-r37, BELOW every clone
  local (@N ascending = vid descending: @770 -> r51 .. @807 -> r37), so `len` as an own local ranks below the class leader and ret
  takes r26, len r25 = the target. Inlining AddRead's body into the loop with the (already declared, unused) `r` local: 54w (sfd
  drops a level). The M3 `#pragma dont_inline` on SFMPS_ExecServer is still needed (without it ExecServerSub is inlined, 116w).
- **adxsje_encode_data 39 -> 0 (pure C, three edits):** (1) the parameter is `ADXSJE sje` (no `void *obj` + kept copy): the
  pre-RA scheduler put the hoisted `lis` magic into cycle 0 next to the param copy `mr r32,r3` and BEFORE the user copy `mr sje,
  obj`, so obj (r3) was live across the lis (-> r4) and the post-RA peephole then forwarded `mr r29,r3; lwz r30,0xc(r29)` to
  `lwz 0xc(r3)`; with the direct parameter the lis follows `mr r29,r3`, takes r3 and blocks the forwarding = the target. (2)
  read_pcm declares `SJ *sji = sje->sji` BEFORE `cnt` (helper locals: first declared = higher @N = lower vid = coloured later ->
  sji r24 below cnt r25). (3) the memset loop and loop D are two static helpers (`adxsje_pad_pcm(sje, bufs, n)`, `adxsje_encode_
  blocks(sje)`): their `ch` locals are clone locals, coloured before the loops' IV @temps (memset trio ch r19 > bufs-ptr r21 >
  n*2 r22; loop-D ch r25 coloured before sji/the read_pcm IVs were handed out). pass 36's "memset loop as a helper 64w" was
  measured on the old prologue form.
- adxsje_write_end_code 2w unchanged. The 2.7 backend forwards the `sth v` store into the load at site 2 (`extsh r0, r31`: r31 =
  n survives the call) but not at site 1 (r0 clobbered by the call -> `lha`); the GC/2.6 debugger keeps `lha` at both sites. Site
  1's pre-RA order is `lha; mr r3; lwz ck.data` in ours (initial code lha, lwz, sth: RHS value before LHS address) and the target
  has lwz first. Spellings that do not change it: `((Sint16 *)ck.data)[0]`, `((Sint16 *)src)[0]`, both indexed, `*(volatile Sint16
  *)ck.data` (LHS), `Sint16 s = *src; *d = s`, `+ 0`, `(Uint8 *)ck.data + 0`, `(Sint16)*(Uint16 *)src` (3w), a `Sint16 *d = ck.data`
  before the if (26w), `*(volatile Sint16 *)src` (7w).
- sfmps_DecodeOneUnit 13w unchanged (M1 pin kept). Read off the dump: cnt (r41, 19/31) is level 2 and coloured before bufin
  (r44, 26/26) and dst (r43, 24/24) which are level 1 -> cnt takes mps's dead r23, bufin/dst new r21; the target's cnt r21 =
  coloured AFTER the r21 holders, i.e. cnt in level 1 (needs 3 fewer neighbours: r0,r3-r12, r31+ghost ret, sfd, len, nbyte,
  nskip, wk, p, ok, hn and the 10 hn-block temps) or bufin/dst in level 2. `unit` r3 / IsZero's `p` param r4 (ours the reverse):
  unit is a CSE @temp (lowest ids), the clone param the highest id of level 1. Tried: `unit` own local (substituted, 13w),
  IsZero(data, unit) (13w), the skip arm as a helper (not inlined, strip_unused fails), the scan loop as a helper (58w, param
  copies), `if (total < 0)` after the loop (159w), pins unit r3 / cnt r21 / both (157/133/88w, level shifts).
- **DecodeOneUnit, the M1 pin's cost measured: the pin `asm { mr r31, err; mr ret, r31 }` makes ret a coalesced ghost (r48 -> r31),
  so every node live with ret has BOTH r31 and the ghost as neighbours (+1 against a real ret node).** That +1 is what lifts
  data/len/nbyte/nskip/wk to level 3 (no pin: data 28 at round 2 -> everything removed in round 2, sfd alone L3 -> sfd r31, 155w
  and the `ret = err` copy folded, size -4), and the same +1 keeps cnt at 31 (level 2, r23) where the target's cnt is level 1
  (r21 = the lowest free after bufin/dst were handed r21). chaitin.py says ONE fewer cnt edge gives the target (cnt r21, all
  else unchanged), but see the model caveat below: the compiler needs cnt at <= 28 by its own rule. Levers that do not work:
  `q = p + len` / `v` for the three kind/xsize/x14 loads as own locals declared last (the temps stay as nodes, 30w/13w),
  `asm { mr ret, err }` (142w), pins through r0/r12 (350/171w), a mps pin r23 (166w: reserves r23, stmw r20; `asm` on `mps`
  right after `mps = wk->mps` fails with "not assigned to a register", placed before MPS_DecHd it compiles), total r22 / bufin
  r21 pins (162/156w), casts on call arguments to create ghosts (no ghost: `(Uint8 *)`, `(void *)`, `(Sint32 *)0`, `(Sint32)f()`
  all identical objects). What the target must have: ret r31 WITHOUT a pin = ret/wk/nskip/nbyte/len/data/sfd all surviving
  round 2, i.e. data at >= 29 there (one more neighbour that is not adjacent to cnt) -- a ghost or a value live in the
  prologue/syshd region only. Not found.
- **chaitin.py removal-rule caveat (measured on 13 dumps):** the model's ascending scan with in-pass decrement reproduces the
  compiler on 11 dumps (0 mismatches) but NOT on v1/v3 (an extra own local declared LAST, adjacent to cnt): cnt at 31 total with
  three lower-id removable neighbours (v/q, ok, hn) is NOT removed in pass 1 by the compiler (19/31, pass 2) although the model
  removes it at 28; in n1 (no pin, cnt 30 total, two such neighbours) the compiler DOES remove it at 28 in pass 1. Snapshot
  semantics (degrees frozen at the pass start) fits v1/v3 but breaks n1/ess/ed (24/8/18 mismatches); descending or
  declaration-order scans are worse. So the in-pass rule is right in general and the case "a newly added lowest-id own local
  adjacent to the node" misbehaves -- the third decrement did not count. Open; keep using `chaitin.py --check` and trust it
  only when it reproduces the dump it is fed.
- **cftfx CFT_Argb420ToArgb8 38w, both mechanisms named:** (1) prologue (the 8-byte size gap): the target keeps `lwz r5 pln.y;
  lwz r4 pln.cb; mr r9, r4 (cb); mr r6, r5 (y); add r5, r5, r3 (a from the temp)`. In ours the frontend leaves `y = (@211 =
  pln.y)` + `a = @211 + half` (a2 dump: no forwarding in the AST), and the RA gives @211 y's colour r6 (the copy is a
  compiler copy with the source still live: no interference edge is added at a copy, so the biased colouring coalesces
  them); cb's copy survives only because @212's colour r4 is taken by `ar`. The target's y and @211 interfere = y (or
  @211) is DEFINED while the other is live: y needs a second definition between the copy and `a`'s use of the temp, or
  the temp must be defined after y. Every ordering of the four statements, `Uint8 *py/pcb` locals, `register`, `y++; y--`,
  a repeated `y = pln.y` (dead first def removed) give the same object; `y = a - half` keeps the copy but as a real `subf`
  (22w, size equal). (2) the 7-word loop tail: the pre-RA schedule (backend-10) IS the target's order (cb, cb2, cr, cr2, j,
  ar, gb increments interleaved with the y/a chain); backend-11 peephole-forward then sinks the `addi cb/cb2/cr, 2` (each
  followed by a non-increment instruction) to the block end and strips their line numbers -- pass 38's IV-increment sink,
  here on three of the seven pointer increments (cr2/j/ar/gb, followed only by other increments, stay). The target was not
  sunk. Independent of the y/cb copies (b8 keeps the tail words).
- cftfx UserTable 135w: k8 (four `w4 = ywidth - 4; y += w4;` defs) with `#pragma opt_loop_invariants off` around the function
  still places the surviving web's `subi` in the preheader (the frontend's placement, as pass 36 found). Unchanged.
- cftfx StaticV 36w: ours DOES reschedule B0 post-RA (flag 0x5 -> 0xd, backend-14) but keeps `stwu` in cycle 0 with the
  callee-saved `stw`s right after it, while the target issues `lwz r9, 4(r4); li r6, 0` BEFORE `stwu` and scatters the two
  saves 10 and 20 instructions later: in the target the parameter loads do not depend on the frame store, in ours they do
  (the same compiler treats `stw r24, 0(r6)` -> `lwz 0x1f74(r3)` as dependent in DecodeOneUnit, both builds). UserTable's
  target has `stwu; li; lwz; stmw` (stwu first, load before the saves). Not understood; not touched.
- adx_sje write_end_code: `#pragma scheduling 601/603/604/750/7400/7450/on` for the function: 68/39/24/2/2/25/2w. An inlined
  `adxsje_st16(ck.data, *(Sint16 *)src)` helper (arguments left to right) still gives the initial code `lha; lwz; sth`
  (value before address): the store's address load cannot be ordered first from C here.
- Tree: src/lib/sfd_mps.c (ExecServerSub form, header comment), src/lib/adx_sje.c (encode_data + two helpers, read_pcm
  declaration order, header comment). objects.py untouched (no unit IDENTICAL). `ninja -k 0` + `dtk shasum -c` 111 OK.
  (End of CRI pass 43. The idEditUnit bullets that follow were appended by the t_id agent while this section was the file's tail.)
- **idEditUnit 15 -> 9 (case-2 arm's three `lbz/stb` copies r9/r0/r9 as the target).** local-alloc.c block_alloc's `case 3` sort
  compares QTY NUMBERS 0/1 and 1/2, not `qty_order[]` entries, so with exactly three block-local qtys q0 is allocated first whatever
  the priorities (q0 < q1: two exchanges cancel; q0 >= q1: none) -> the `no` temp (pri 5000: `mr r3,r30` sits inside its pair) gets
  r0 in ours. The target's level r0 / no,parentNo r9 is the qsort order (level 10000 first, then no conflicts with it): any 4th local
  qty gives it. Applied `{ int t; asm("" : "=r"(t)); asm volatile("" : : "r"(t)); }` after `toolIdDataInit(d)` (LADBG confirms; a dead
  `asm("" : "=r"(dead) : "r"(d))` is deleted by flow, a reused `int a` for no/parentNo has 2 deaths -> not local (REG_N_DEATHS == 1
  rule at local-alloc.c:406) and global gives it r0). The natural 4th qty is not found.
- idEditUnit head (9w left: `lis r9/lbz r10/addi r11` + `extsb r0` + `andi. r10`): with the `register JOY* joy asm("r11")` pin the
  Joy high inherits r11 through local-alloc's dying-operand suggestion (S q0 in block 0); unpinned it is r9 by itself (class
  BASE_REGS excludes r0) and global's order is extsb value (pri 4000) r11, joy (2181) r10, load (1666) r8 (GDBGV=1 masks: r0 is
  taken by pseudo 159 = `joy->trg` in case 0's `zero_extract` compares, r9 by the high). **The target's `stb r10,0x5d` (case 2,
  `grpSw = 2`) stores the QI LOAD pseudo, not the extsb subreg:** `w->grpSw = w->editStep;` in that arm makes the head IDENTICAL
  unpinned (the load gets 4 refs -> r10, extsb r0, joy r11) but reloads editStep (`lbz r0,0x3`, 27w) because the join label
  `.L+2bc` (after `stb r25,0x5d`) ends cse's ebb; the target's compiler knew load == 2 there (record_jump_cond only records the QI
  equivalence for a PARADOXICAL subreg compare, so its switch operand reached cse as `(subreg:SI (reg:QI load))`, not our
  `(sign_extend)` pseudo). `u8 step = w->editStep; switch ((s8) step)` adds a clrlwi (55w). Open: the spelling that makes the
  switch index a paradoxical subreg of the byte load while keeping the `extsb`.
- Not touched this pass: toolIdOption 12w (pass-5 residue: `lbz lang2`/`lbz lang` order and the `optMenuName` giv-init `lis`),
  the 2/2/2/4-word rows of `_._6cCoord/_._7ID_DATA/_._5cUnit/__static_initialization_and_destruction_0` (bytecmp still counts them
  after the toolIdInit fix, so they are not .text-shift artefacts: `lis r9,lbl_t_id_bss_14DAC4@ha / addi` vs ours `idClip+0x2fa00`
  — same final address 0x14DAC4 = idClip + 0x2fa00 (idClip = 0x11E0C4), so it is the reloc SYMBOL/addend split that bytecmp scores;
  look at how bytecmp resolves a symbol+addend against an anonymous bss label before spending time on the source).
- Note for the catalogue row "same block, two independent insns in the other order": sched2 re-sorts equal-priority stores by
  dependents count, and a store's source HARD register rewritten later in the block adds one (anti) dependent — the sched1 order
  is only kept within each group. For a `li` argument that must precede an `addi fmt@l` (sched1 tie decided by INSN_REG_WEIGHT: the
  lo_sum's dying high gives it weight 0), an asm-emitted li with a DYING pseudo input (`"r"(i)`, a dead loop counter) is the lever;
  the input must be a value whose extension does not change global's order.

### CRI pass 44, continued (the lever's real mechanism; sfd_mps DecodeOneUnit 13w / cftfx / sfd_tst SFTST_Calc: no pin applies; Ste4AsSte model reading; 2026-09-12)
- **CORRECTION to the lever row above ("reserves nothing"): the pin register is taken out of the colour set for the WHOLE function whether or not the target uses it.**
  sfmps_DecodeOneUnit (target never uses r9-r12): every single-`mr` pin - on `unit` (a new own local for `sfd->prm.unit`), `p`, `cnt` (before/after the scan loop), `len`
  (entry), `delim`, `wk` - to r9/r10/r11/r12 or r6/r7/r8 gives the SAME 124w (mps r23 -> r29, every level shifts), r5 132w, r4 148w, r3 157w, r0 225w: the effect
  is the removed colour (K 29 -> 28), not the pinned value's ghosts. Replacing the pass-29 hard pin `asm { mr r31, err; mr ret, r31 }` by `ret = CopyPketData(..);
  asm { mr rV, ret }` (rV = r9..r12, at the def, at `return ret`, or at the entry `ret = 0`) = 134w (no pin 142w): the hard pin works here because `err` is
  call-defined, so `mr r31, r3` is a REAL copy and `ret` coalesces into the physical r31 (a pinned value takes the pin register only through a kept copy). The 13w
  residue (cnt r23 vs r21 = one neighbour too many; IsZero psize/p r3/r4 order) needs FEWER neighbours: no pin form. Unchanged (the pass-43 agent's ExecServerSub is
  IDENTICAL now, 25/26).
- **Where a pin IS free: a register that is live for the whole function anyway.** Ste4AsSte: r3/r4/r5/r7 (stepped/returned parameters = coalesced ghosts) and r6/r8
  (histl/histr: propagated parameters, live to the exit stores). Pinning to r6 makes histl a coalesced web (`r35->r6` appears in the ghost list) = +1 neighbour on
  every node live in the loop; chaitin.py then diverges (64) from the compiler's dump, so pinned graphs must be judged by bytes. cftfx StaticV/UserTable/Argb420 and
  SFTST_Calc use every volatile register in the target and their parameters die early: every pin there costs a callee-saved register (StaticV: `stwu -0x20`, r29 saved,
  +8 bytes, 98-110w vs 36w; UserTable r3/r4/r8 +8 bytes 191-193w; the `tbl` register r5, live throughout, is free but changes nothing, 135w). SFTST_Calc (r12 unused
  in the target): `asm { mr r12, ave@hiword }` / `@loword` after `ave = SumHist / range` is deleted (size equal) but 101w (79w before): `ave` must LOSE a level (pass 40),
  pins on `diff@hiword` 101w, `diff@loword` 139w (-4 bytes: a copy emitted), `adiff@hiword` 101w, `excess@hiword` 105w. Nothing applied in sfd_mps / cftfx / sfd_tst.
- **adx_sje encode_data is IDENTICAL in the tree (pass-43 agent; 16/17, write_end_code 2w left = the post-RA dual-issue tie of pass 36).** Not touched.
- **Ste4AsSte, the target's level structure derived from the model (asm-defined `qtbl` dump = own-local vid r50, chaitin.py IDENTICAL):** the L2-sweep degrees at the
  node's turn are scl 35, smul 34, sadd 32, q_r 25, q_l 25, nblk 30, t 25, qtbl 32; the two `>>4` temps r93/r99 are exactly 29 in L1. N(nblk) is a subset of N(sadd)
  (sadd has one L1 temp more), N(qtbl) = N(sadd) + {r0, r1, the `r72` ghost, one L1 temp}. The target's colouring (sadd/smul/scl L3; l2..sc_r, qtbl, s, t, nblk L2 in
  that order; `>>4` temps L1 in place) follows from ONE fact: the `>>4` temps must be L1 (28, not 29). Then nblk/sadd/qtbl each lose 2 in L1 -> sadd 30 (stays L3),
  nblk 28 at its turn (L2, removed after q_r/q_l -> coloured last = new r19), qtbl 33 - q_r - q_l - t - nblk - s(if L2) = 28 (L2, r22 after sc_r); plus s L2 (+5,
  a pin) for the r21 slot. `d >> 4` loses `dr` when `dr = src[0x12]` is read AFTER l2's clamp (dump ra_drlate: r95 -> L1, 28) - the post-RA scheduler hoists the
  `lbz dr` back to the target's position, so pass 39's "pre-RA order verified" was the SCHEDULED order; the graph is built on statement order. `dr >> 4` (r101) keeps
  29: its neighbours include `d` (live to idx_l) and `dr` itself (live to idx_r) and the three right-prediction temps c1*r1, c2*r2, add (the shift is evaluated
  before the prediction). Spellings measured on the 41w tree: dr late 120w, src++ late 63w, q_l before t 51w, prediction through `t` (`t = (c1*l1 + c2*l2) >> 12;
  l2 = (d >> 4) * sc_l + t;`) 114w / both channels 97w / with dr late 119w, `key` as the prediction temp 97w, `r2 = t` moved before the L1/R1 lines 58w, a dead
  `asm { mr r8, t }` at the outer/inner loop top 66-114w. Asm-defined `qtbl` (`asm { lis qtbl, AdxQtbl@ha; addi qtbl, qtbl, AdxQtbl@l }`, the own-local vid) alone
  115w, + `asm { mr r6, qtbl }` 33w (L3 = smul r0 / qtbl r11 / scl r12, sadd r22, t r21; the `lis`/`addi` sit at the asm's position, not the target's 0x7c/0x90) -
  not adopted over the 41w pure-C + one deleted `mr` form. Open: a statement shape in which `dr >> 4` has 28 neighbours (one of d, dr, the prediction temps not
  live across it) with the target's scheduled order; then `s` needs the +5 (a pin to r3/r4/r5/r7).
- Tree: src/lib/adx_dcd5.c Ste4AsSte only (the `register const Sint32 *qtbl` + `asm { mr r6, qtbl }` tagged M1 neighbour pin; locals r1/r2 renamed rr1/rr2 to silence
  the "ambiguous use of local variable(r2) and assembler register" warning the asm triggers; bytes unchanged). adx_dcd5 2/4, 41w + 180w. objects.py untouched, nothing
  flipped, no `ninja -k 0` needed. Harness ~/.cache/cri44 deleted; the kit untouched.

### CRI pass 44, part 3 (adx_dcd5 Ste4AsSte 41 -> 25w and Ste4AsMono 180 -> 143w APPLIED, both pure-C statement splits found from the model; sizes now equal; 2026-09-12)
- **The model's prediction held: the `>>4` temps are exactly 29 because the shift is evaluated BEFORE the prediction sum, so it is live across the three prediction
  temps (c1*x, c2*y, add).** Splitting the statement, `t = (dr >> 4) * sc_r; t += (c1 * rr1 + c2 * rr2) >> 12;`, evaluates the product first: the shift dies at the
  `mullw`. In pure C alone this changes nothing (115w: the frontend re-merges the two statements, the dump still shows r101 at 29), but on top of the `asm { mr r6, qtbl }`
  pin it gives **25w** (all of l2 r31 .. sc_r r23, s r21, t r20, nblk r19, both `>>4` temps r21/r20 in place, `stmw r19`); the same split on the left channel or on the
  l1/r1 lines is worse (96-114w), the asm-defined `qtbl` on top gives 17w (only the L3 set and the prologue positions left, but the `lis/addi` cannot move to the target's
  0x7c/0x90 because C statements do not cross an asm). Residue 25w = the L3 membership: ours smul r0 / scl r11 / qtbl(temp) r12 with sadd L2 -> r22; target sadd r0 /
  smul r11 / scl r12 with qtbl L2 -> r22; plus one temp pair swapped (`mullw c1*t` r21/r26 vs `mullw c2*rr1`) that follows from it. Pin positions inside the loop or after
  it are NOT deleted (+4 bytes, 162w); before `nblk = nfrm / 2` 26w; after the hist loads 25w. The r6/r8 effect is independent of the pinned value (s 41w, nblk/i/l1/t
  25-26w): it is the histl/histr ghost web (+1 on every loop node), not the pinned value's ghosts. Pure-C attempts to keep `histl` a web (`*histl++ = l1`, a dead
  `histl += 2`, `l1 = *histl++; l2 = *histl--`, a `register Sint16 *hp` copy) are all folded/propagated (114-115w).
- **Ste4AsMono 180 -> 143w, pure C, size 0x2f0 = target: `r1 = sc_r * AdxQtbl[dr & 0xF]; r1 += (c1 * t + c2 * r1) >> 12;`** (the right channel's second sample only;
  the same split on l1 218w, on t/l2/m 180w, l1+r1 218w). The dump (chaitin.py IDENTICAL): no spill pick any more (L4 sadd/smul/scl, L3 the two sc temps r77/r88, the
  c-ext, l1 l2 r1 r2 nblk i d dr m, L2 five `>>`/`rlwinm` temps + t) where the target is stuck twice (nblk r18 then smul r19 picks; l2 in r12 = an L1 node there). r6/r8
  pins on nblk 136w, l1 138w, i/r2 204-210w (+4 bytes) - not applied. `r2 = t` after m's clamp 148w, at the body end 158w, `c1 * r2` in r1's prediction 143w, table
  operand first 143w, `Sint16 sc_l/sc_r` own locals 143w, `m = l1 + r1; m = m * 7 / 10` 143w.
- Tree: src/lib/adx_dcd5.c Ste4AsSte (split + pin, comment updated) and Ste4AsMono (split). adx_dcd5 2/4, 25w + 143w, .text size equal. Nothing flipped.

### CRI pass 46 (mps_lib Matching 6 -> 7/7: MPS_Create 2 -> 0w pure C, the RA's dead-def deletion clears the scheduled bit; adx_tsvr nlp_trap_entry 2w: a physical-register pin RESERVES the register function-wide, r0 included; sfd_cre AnalyMpv 15w: the target's block IS post-RA rescheduled (record-form merge dirties it); sfd_tst SFTST_Calc 79w: MulDiv-as-own-local negative, model says +9; 2026-09-12)
Harness ~/.cache/cri46/ (deleted). Kit extended, not copied: **`tools/research/mwccdbg/blkflags.py DUMPDIR`** = the per-block `:{xxxx}` flag word after every
backend pass >= 11 (the pass-38/41 flags.py, now persistent; README updated). Read it FIRST for any "instruction order within a block" residue.
- **mps_lib `MPS_Create` 2 -> 0 (pure C, flipped, 111 OK): a third clearer of the scheduled bit = the register allocator deleting a dead def.** blkflags: the
  store block B10 is `400c` through backend-13 and `4004` after backend-14 (regalloc). The nested `for (i < 3) for (j < 8) syshd[i].raw[j] = -1` is unrolled
  in two stages: the frontend unrolls the inner loop, the backend's loop transform unrolls the outer one and leaves its init `li r32,0` (`i`, r32 = the first
  own-local id) as a dead def in B10; the RA deletes it (B10 before/after: only that `li` gone), the block is dirty, the post-RA scheduler re-runs it and
  orders the cycle-3 filler pair `addi r0,r3,@l` before `li r4,-1` (pass 37's WAR-edge priority). The target = the PRE-RA order (`li r4,-1; addi r0`), i.e.
  the vendor's block was never dirtied: three separate `for (i = 0; i < 8; i++) mps->syshd[K].raw[i] = -1;` loops (each fully unrolled by the frontend, no
  counter survives) give it, IDENTICAL. Negative: one flat 24-iteration loop (not unrolled, 108w), a post-loop use of `i` (64w). Rule: a fully frontend-
  unrolled loop leaves no counter; a backend-unrolled loop leaves the counter's dead `li 0` for the RA to delete = a dirty block.
- **adx_tsvr `adxt_nlp_trap_entry` 2w, the pass-37 "r0-coloured neighbour" angle closed negative: a pin to ANY physical register reserves it for the whole
  function, r0 included.** `register Sint32 o = ofst; asm { mr r0, o; mr o, r0 }; ofst1 += o` moves the `lha` to r4 as wanted but every other r0 temporary of the
  function (`lbz r0,0x98`, `li r0,0`, `lwz r0,0x28`, the decsmpl `add r0`) moves to r3/r4 (11w); the same with r4 shifts the argument temps (8w); r11/r8 pins leave
  the 2w (their ghosts are r11/r8-coloured, they block nothing the lha wants). So the codeless neighbour pin only works with a register NOBODY in the function
  takes, and the r0 neighbour of the target's join block has no C source here (pass 37's reading stands). Own-local `o` without a pin, `ofst2v` first, `o` used
  twice in the condition: 2w/2w/13w. Left 2w.
- **sfd_cre `sfcre_AnalyMpv` 15w: the question "did the vendor's block skip the post-RA reschedule" is answered NO.** blkflags on ours: B6 (the byte-load block)
  `200c` after the pre-RA schedule (11), `2004` after peephole-forward (12: the `addi data,data,1` sink to the block end, pass 41's rule; no store in the block can
  stop it), rescheduled at 17. But the block ALSO contains the `rlwinm r50; cmpi r50,0` -> `rlwinm. r0` record-form merge, and that merge (backend-16 peephole)
  clears 0x8 by itself (B9 `000c` -> `0004` at 16 with nothing else in it). The vendor's B6 had the same merge (target `extrwi. r0,r5,4,24`), so the target IS a
  post-RA schedule, whatever the sink did. The target's colours then read the vendor's PRE-RA order: `ofs+1` r0 and the rlwinm temp r0 = no interference = the
  addi was not issued between `rlwinm` and `cmpi`; b5 r6 = ofs's register = `lbz b5` issued after the addi; b8 r8 (ours r4 = ofs+1's) = `lbz b8` issued before
  `subf size`. Ours pre-RA: `lbz7|subf; add|lbz4; rlwinm|addi; cmpi|lbz5; lbz6|subf; lbz8|addi data; lbz9 ..` (the addi takes cycle 3, pass 38's rule), so the
  vendor's `addi ofs+1` was not ready or not preferred at cycle 3 — still the open question; c1 (b6 statement before b5) swaps the two loads in the output =
  raw order decides equal-height load ties, so the target's `lbz 6; .. lbz 5` may also be a source order. Probes: b6/b5 swapped 15w, `q = ofs + data; data = q + 1`
  (dst != src, no sink) 16w, `size -=` before the data update 14w, `data = p; data++` 24w (-4 bytes). Left 15w.
- **sfd_tst `SFTST_Calc` 79w: "raise MulDiv" is not reachable, measured and modelled.** (a) `(msec = UTY_MulDiv(..))` inside the argument list with a plain
  local is propagated away (79w, same object). (b) `msec = UTY_MulDiv(..);` as a statement moves the @171-@174 loads below the call (85w; the target loads them
  first = the call is an argument, pass 40). (c) `register msec` + neighbour pin: after the sprintf (msec then lives across the call, coloured first of the group,
  NEW r30, 107w, +4 bytes), before the sprintf on the not-yet-defined msec (a separate dead web: msec gets a NEW r20, `stmw r20`, 122w), pinned to r3 (383w),
  r12 statement form (106w). (d) chaitin.py on the dump: MulDiv (r403, 20/38) needs **+9** never-removed neighbours to be coloured before ave.lo (then r24 as the
  target), and even then the sprintf group stays out.hi r21 / out.lo r22 / mt.lo r23 (target r22/r23/r25): the target's group was coloured BEFORE tol.lo (@123)
  took r21 and MulDiv sits INSIDE the group between out.lo and mt.lo — a frontend temp with an id between @171 and @172 (pass 35's conclusion), which the
  right-to-left second-sighting CSE numbering (@171 out.cnt, @172 mt.cnt, @173 mt_max, @174 hlp.cnt, read off frontend-01) cannot produce for an arg-3 call.
  Not closed; no pin applies (pass 44's finding holds).
- Flags: `lib/mps_lib.c` True (CRI pass 46 block in objects.py), 111 OK. sfd_cre 5/6, sfd_tst 10/11, adx_tsvr 5/6 unchanged, no tree edits there.

### CRI pass 45 (cftyp422_ppc Y84C44 29w read to the pass: residue (1) is backend pass 10 "code-motion", not pass 07; every pin register is reserved function-wide in a function whose asm blocks span both loops; residue (2) = cw needs one r6 neighbour + cw declared before cw3 (model-confirmed), no source form found; nothing applied, not flipped; 2026-09-12)
Harness ~/.cache/cri45/ (`v.py NAME 'OLD=>NEW'..` literal edits of the tree source through variant.sh, `--full` diff lines, ra.py dumps ra_*; deleted at the end). Kit only. No tree edit, objects.py untouched by this pass, nothing built under the lock.
- **Residue (1) mechanism, read off the dumps:** `n = cnt` (`mr r65, r55`) survives passes 03-09; pass 05 loop-code-motion does not hoist it because `n` has a second def in the loop (the `n--`), the loop transform (06) emits `mr r155, n; cmpi n, 0; ... andi. r155, 3` and a dead `addi n, n, -1` at the exit, 07/08 delete the dead addi, and **pass 10 "code-motion" hoists the now single-def `mr n, cnt` into a NEW preheader block (B27) and rewrites every use of n (`mr r155, n`, `cmpi n`) to cnt**; the dead `mr r65, r55` reaches RA as a 0/0 node. So the copy is the backend temp r155 (vid above every own local -> coloured at index 23 -> r9 before ywidth/yw3); the target's copy is coloured after yw3 = an own-local vid = a copy that pass 10 did not hoist (dest live-in or source redefined in the loop). Not found: a form where that holds for free. Probes: `asm { mr n, n }` after the copy or `asm { mr cnt, cnt }` (deleted before pass 10, 29w unchanged); `asm { mr n, n }` at the body end (95w: kept n live, still hoisted); two-def `n = cnt` pre-loop + body end (69w, size +8: `mr r4, r9; mr r6, r4`, cnt r9); `#pragma opt_propagation / opt_loop_invariants / opt_dead_assignments / opt_common_subs / opt_strength_reduction / opt_unroll_loops / opt_vectorize_loops off` around the function: NO effect at all in 2.4.7 -O4,p (`#pragma optimization_level 1` does change the code, so the pragma scope is right); a hoisted count `n = ywidth / 8` in the body would put the COUNT at n's vid (degree 80, still r4) and leave the copy a backend temp = no change (model), not built.
- **Pins are unusable in this function (measured):** a physical register named in any asm block is live from the first asm to the last (pass 42), and Y84C44's dcbz/dcbt/stfdu asms span both loops, so `asm { mr rV, x; mr x, rV }` reserves rV over the whole function whatever x and wherever placed: n pinned to r4/r6/r8/r9/r10/r11/r12 after the copy / at the body end / after loop 1 = 126-140 / 175-182 / 224-310w (size +8 after loop 1: the mrs are emitted), cnt pinned = the same numbers, cw pinned to r6/r9/r10/r11 at three positions = 134/126/127/125w (identical to the n pins: the reservation dominates). This reconciles pass 40 ("reserves nothing": cvFsGetFileSize had no other asm) with pass 44 ("reserved for the whole function": Ste4AsSte has asm). Rule: the codeless neighbour pin needs a function with no other asm blocks or a register free over the asm span.
- **Residue (2), the model (chaitin.py on the cw-before-cw3 dump):** with `Sint32 cw;` declared before `cw3` (cw vid r44 > cw3 r43) plus ONE extra edge from cw to any node coloured r6 (r151 = the hoisted `cw3 + cskip` add, r89 `height + sign`, r91 its `srawi 3`, r93 = cw's own `srawi 2` operand) the prediction is cw r9, cw3 r10, o1/o2/o3/steps unchanged = the target; the declaration move alone = 29w (cw still takes r6 in place). Pre-RA (backend-11) and post-RA (backend-17) orders of the setup block are identical in ours: `addze cw; mulli cw3; rlwinm o1; rlwinm o2; add r151; rlwinm o3; rlwinm r152` (o1/o2 fill the mulli latency, the add waits for cw3, so cw dies before the add). No source form added the edge: cw3/o1/o2/o3 in all 12 orders (29w), `cstep = cw3 + cskip` as an own local at 3 declaration positions with `cbp0 += cstep` (29w: statement 256 is still scheduled after 257/258), o1/o2/o3 hoisted from body pointer arithmetic `crp1 = crp0 + cw; crp2 = crp0 + cw * 2; crp3 = crp0 + cw3` (35w: same colours, the body adds flip to `add r9, r3, r28` = the original wrote `offset + pointer` as ours does), `cw` statement before `hblk` (31w, with the declaration move 34w: the hs chain interleaves, cw r12). What the target must have had: a use of cw (or a compiler copy coalesced into it) that is live past the add or past hblk's `srawi` in the PRE-RA order and gone after RA, or a post-RA reschedule of that block (no `mr` in ours = the block keeps its schedule).
- Flags: `lib/cftyp422_ppc.c` stays False (7/8, 29w). Tree unchanged from pass 42.

### CRI SWAR kernels pass 10 (mpv_mcy 16x16 4p 136 -> 122w APPLIED: the five loop variables take the target's r0/r3/r4/r5/r6 when the pixel words are OWN locals declared before them; the masked-load split and the own-local pixels exclude each other; H2/V2 225w and mpv_mc 72/73/436w unchanged; nothing flipped; 2026-09-12)
Harness ~/.cache/cri_swar10/ (`gen10.py NAME decl=px,ps,lv lvorder=.. init=.. pix=u32|u8|u16|s32m mask=none|self|use|use8|ldm sum=cast|cur
lorder/sorder/inter/ba p8=pre|mid|mid2|post reg=.. split=..` whole-function generator; `rr.sh NAME opts` = gen + ra.py dump + `cnt.py` (initial counts
up to d[0]/d[1] and the split verdict) + words + `blk.py` block-1 compare + `sum.py` (chaitin levels, loop-var colours, scan-1 survivors) +
`osearch.py` (offline chaitin search over own-local scan orders against the target's colours mapped by symbolic value, `colmap.py --show`);
`symb.py LST` symbolic trace of a listing). The pass-9 harness is deleted at the end of this pass. Loops in zsh: `./rr.sh ${=a}`.
- **Why the target's stride/d/i/s0/s1 are r0/r3/r4/r5/r6 (ours r9/r12/r8/r10/r11):** chaitin.py (IDENTICAL on z1) puts them at level 2 with 8
  backend temps + p2/p3/p4 (d removed at degree 25 = 4 loop vars + 12 pack ghosts + r0 + 8 L2 temps); the last scan removes in vid order, so
  every higher-vid L2 temp pops first and takes r0,r3..r7. The target's five pop FIRST = they alone form level 3: at scan 2 their degree
  must stay >= 29, i.e. >= 4 more scan-1 survivors visited after them. `decl=px,ps,lv` (pixels and sums declared BEFORE the loop
  variables, so the p's have higher vids) lifts i/s0/s1 (d3, 130w; d 28 and stride 27 are 1-2 short); unmasked `Uint32` pixel own locals
  declared first (d5: `pix=u32 sum=cast`) lift all five to L3 -> stride r0, d r3, i r4, s0 r5, s1 r6, `li r7,0x10` ctr temp, identical
  prologue, 122w — but without the masks the block does not split (B3 = 123, the check fires before d[17]).
- **Split arithmetic (cnt.py):** with pixel-9 loads + p8 in block 1 the count before the `d[1]` statement is 65 + E (+8 for d[1]) and the
  boundary lands after d[1] iff E in [28, 35], E = the later-deleted initial instructions of the first half: z1 masks-at-load (20, one per
  lbz) + CSE'd `(Uint32)` casts (12) = 32 OK; own-local pixels + casts (`Uint8`/`Uint16`/`Uint32`, e1/g1/d5) = 14-20 (only the two-use
  pixels' CSE'd extension survives as an `mr @t, a`; copy-prop keeps the OWN local because the copy's source is the local); per-USE
  `(Uint32)(Uint8)a` (k4/k5) = 38 (3 over: the split falls before d[1]); `a &= 0xFF` after the load is folded by the frontend (f3: E 20);
  `(a & 0xFF)` at use on `Uint8` (g4) = 34 but breaks one pack fusion (78/79 pre-schedule, 141 lines). The mask AT the load (`s[k] &
  0xFF`, any pixel type) always turns the pixel into the load's backend temp (`rlwinm a,t` -> `mr a,t` -> a replaced by t, across
  blocks), so masked pixels are visited after the loop variables in every scan and cannot lift them. p8's position (before d[0] / between
  the stores) does not change E.
- **Pre-RA schedule vs post-RA:** z1 (masked, split) and d5 (own-local pixels, unsplit) have IDENTICAL pre-RA schedules for the whole first
  half (symbolic compare of backend-12/08 B3); their final block-1 orders differ (29/67 vs 27/67, load order 10/20 vs 18/20) only through
  the post-RA reschedule under different colours (e.g. target `add a1+b0` at 7 before `lbz a3`: the WAR on r8 from `add r8,r7,r22` at 9
  raises its priority). So the residue is colours -> post-RA order; the pre-RA order is the same in every spelling tried.
- **Offline colour search (osearch.py):** with z1's/o1's interference graph no own-local scan order (loop vars x p order x 8 pixel orders x 8
  group orders) reproduces the target's block-1 colours (best 24/54 nodes, o1 `inter=1` + `ps,px,lv` bbaa); the target's long-lived pixels
  (a3 r21, b2/b4 r22, a4 r26, a5 r27, b5 r28, b6 r25, b8 r24, b7 r21) are callee-saved while its short sums recycle r7/r8, i.e. the target's
  pixels popped LAST (own locals at level 1) after the sums took the volatile registers — consistent with own-local pixels, not with the
  masked backend temps. `register` on locals changes nothing; `psrev` (p8 declared first) moves the block-1 final order (37/67, n4).

### CRI pass 48 (sfd_mps DecodeOneUnit 13w: the unpinned target = ret/data/wk need +3/+1/+1 never-removed neighbours, a `?:` assigned to an argument local is a codeless ghost pair (+1 found, +2 open); adx_sje write_end_code 2w: 2.7 reschedules site 1 where 2.6 does not, LSU-first cycle-0 tie; nothing applied, nothing flipped; 2026-09-12)
Harness ~/.cache/cri48/ (`try.sh NAME` = variant.sh + ra.py + ghosts + chaitin --check; `model.py DUMP [+a:b|-a:b|+ghost:a,b,..]` = chaitin replay
with edited edges and the round-2 start degrees).
- **DecodeOneUnit, the unpinned requirement made exact (n1 = tree minus the M1 pin, `ret = CopyPketData(..)`, 142w, chaitin IDENTICAL):** round-2 start
  degrees sfd 29, data 28, len 29, nbyte 30, nskip 29, wk 30, ret 28; total (22) and mps (27) are removed in round 2 BEFORE wk/ret's turn, so at
  their turns data 28, wk 28, ret 26. The target (ret r31 = L3 with the six) needs data +1, wk +1, ret +3 NEVER-REMOVED neighbours (ghosts or
  physical), none adjacent to cnt (30 total, removed at 28 after ok/hn), at most one adjacent to mps (27 -> 28). Model: three ghosts adjacent to
  {sfd,data,len,nbyte,nskip,wk,ret,delim} reproduce the whole target colouring incl. cnt r21 (bufin/dst r21 handed out first).
- **A coalesced-copy ghost from pure C: `n = (len < 0xB0) ? len : 0xB0;` (n2).** The frontend makes the `?:` a backend temp copied into a frontend
  @temp (`mr @700, r96`), the stores read r96 and the MEM_Copy argument move reads @700, both coalesce into r5 = TWO r5 ghosts where the if-form had
  one (`n` itself), bytes identical, +1 on every node live in the syshd block after GetLastSysHd (data 29, wk 31, ret 29 at round-2 start -> wk L3
  r31, 132w all ARG_MISMATCH). Two more ghosts on ret (cnt-dead region) close it per the model. Negative (no ghost, bytes equal): `(err = IsEndcodeSkip
  (sfd)) != 0` (single-use def propagated), `ok = IsZero(..)`, `delim = (len >= 4) ? CheckDelim(data) : 0` (same as if/else), `go = helper(sfd, delim)`
  (inlined @ret propagated), `shdr = NULL` predef, TermIfInTerm `t = (..) ? (TermOut(sfd), 1) : 0`. Bytes change: go chain as `?:` (v3/v19/v20,
  +-4 bytes, but v3 shows a vreg-coalesced class ghost `r57->r56`). Clone parameter copies and constant/load arguments never make ghosts (propagated /
  materialised directly into the argument register); a call result makes an r3 ghost only when assigned to a variable (`mr @t, r3; mr var, @t`).
  Tree unchanged so far (M1 pin, 13w; pin + `?:` n = 13w).

### CRI pass 47 (adx_dcd5 Ste4AsSte 25w / Ste4AsMono 143w: the pin's K rule makes chaitin.py exact on pinned graphs (`--k`); the Ste residue is ONE neighbour on sadd; Mono needs the level structure, not more pressure everywhere; IN PROGRESS; 2026-09-12)
Harness ~/.cache/cri47/ (`mk.py OUT [--base=B] 'OLD=>NEW'..` literal edits, `try.sh NAME [FUNC]`, `ra.sh NAME [FUNC]` = ra.py dump into ra_NAME + words,
`wste.py`/`wste2.py`/`wmono.py` = chaitin what-ifs (fake never-removed neighbours, ghost on a physical register, vid moves), `pincheck.py`/`ordcmp.py` = model vs compiler
removal order; deleted at the end).
- **Kit extended: `chaitin.py --k N`.** A pinned graph diverged from the model (pass 44) only because every physical register named in an `asm { mr rN, x }` leaves the
  colour set: the removal threshold is K = 29 - (#pinned registers). `--k 28` reproduces the tree's Ste4AsSte dump (order/colours IDENTICAL, cost lines aside),
  `--k 27` a two-pin Mono probe; the ghost lists and totals were never the problem (totals match the compiler in every dump). README updated.
- **Ste4AsSte 25w, read with the exact model (K = 28, the r35->r6 ghost):** the target's colouring is reproduced by exactly two graph changes: (1) the AdxQtbl address node
  sits at the own-local vid r50 (between sc_r r51 and s r49) and does NOT interfere with r0/r1/r72 (r72 = the `lwz r72,0(r1)` back-chain base of the stack-parameter
  loads, coalesced into r1: a node interferes with it only when it is defined before the last stack-parameter load); (2) sadd has ONE more never-removed neighbour
  (physical or ghost). With (1) alone sadd is removed at 27 in the l2..sc_r scan (one below K) and takes r22; with (1)+(2) all 18 named nodes match (sadd r0, smul r11,
  scl r12, c-ext r10/r9, l2 r31 .. sc_r r23, qtbl r22, s r21, t r20, nblk r19). smul/scl have exactly the r1+r72 pair that sadd lacks (sadd is the LAST stack load).
  Not found in C: (1) the frontend emits `qtbl = AdxQtbl` as `lis @a; addi @b,@a; mr qtbl,@b` and backend copy propagation removes the own local (initialiser,
  `&AdxQtbl[0]`, `(Uint32)` casts, `Sint32 *` type, `+ i`/`+ z` with a known-zero variable (frontend-folded even across statements and stores), all the same;
  `AdxQtbl + 1` gives `addi qtbl,@b,4` with the local as destination but costs the index -1 (60w); a second def at the loop tail is +8 bytes); the two-register asm form
  `asm { lis qhi }` `asm { addi qtbl, qhi }` is 17w like pass 44's (asm instructions ARE moved by the pre-RA scheduler: the lis lands above the stack loads, so qtbl
  keeps the r72/r1 edges and stays L3). (2) sadd's extra neighbour cannot be r0 (its own colour), r9/r10 are already edges (ours loads the params before the c-ext
  extsh; the target's final order has the extsh first, but that is post-RA), a histr ghost adds +1 to every loop node (r8 pins: 80w, the model agrees), one more L2
  node adjacent to sadd is impossible (sadd is adjacent to every L2+ node) and keeping an L1 temp alive (+k) triggers a second spill pick instead. Open: the pre-RA
  position of the sadd load (a consumer of sadd inside the prologue block would hoist it above the smul/scl loads and give it the r72/r1 pair; the frontend's `extsh`
  of sadd is deleted at backend-07 constant propagation before scheduling).
- **Ste4AsMono 143w:** the biased-colouring fact first: `r2 = t` leaves its `mr` only when t and r2 get DIFFERENT colours (the RA drops a copy whose ends coalesce
  even when t is used after the copy); the unsplit `r1 = sc_r*q + ((c1*r2 + c2*r1) >> 12)` forms (target semantics: `mullw r20,r9,r31` = c1*r2) are 0x2ec = one
  instruction short (t/r2 share a register); the split form's +4 bytes is that `mr`, so the split is a colouring device, not the vendor's shape. Model findings on the
  unpinned dump (chaitin IDENTICAL): two ghosts on r6/r8 with K = 29 give the target's LEVEL structure (sadd r0 / scl r11 top, smul then nblk spill picks, l2 r12 because
  r12 is free when smul is a pick) but the first two body temps (c1*l1, d>>4: 31/32 total) then survive L1 and take r12/r31, and the hoisted AdxQtbl/0x66666667 temps
  (initial-code vids r116/r105, coloured first in their level) take r9/r10 where the target has c1e/c2e; the target colours qtbl r23 and the magic r22 AFTER sc_l r25 /
  sc_r r24, i.e. below the scale nodes in vid or level. Bytes: r6/r8 pins on nblk/l1/l2/r1 (single or paired) 142w; a Ste-shaped Mono (declaration order l2 r2 r1 l1 i d
  dr, `Sint16 sc_l/sc_r`, own-local `qtbl`, Ste's scale block) 178w, + `asm { mr r6, qtbl } asm { mr r8, qtbl }` 139w with nblk r18, smul r19, l1 r29, l2 r12, c1/c2 r9/r10
  right but scl/i as the spill picks (K = 27 is too much pressure: the model shows scl picked after nblk/smul/sadd/i are gone). `c1 * r2`, `r2 = t` before m,
  two-def t/l1, table operand first: 143w or 180w (unsplit), none changes the level structure.
- **DecodeOneUnit, where the two missing ghosts can and cannot sit (from the n1/n2 graphs):** ret is live everywhere (108 = every node), so the +2
  must be NEW never-removed nodes (coalesced copies) or physical pins, in a region where cnt is dead (anything outside the scan loop / hn block /
  `*nskip = cnt`) and at most one where mps is live (prologue up to GetLastSysHd). Ghost sources seen in this function: (1) a `?:` result assigned
  to a local (`@temp` + backend temp, both coalesce when the local dies at an argument move), (2) a call result assigned to a variable whose def has
  >= 2 uses (`mr @t, r3; mr var, @t` -> the @t is an r3 ghost; a def with one use is propagated, `cmpi r3` directly), (3) an own local dying at an
  argument move without a call in its range (`n` -> r5). The two `mr r31, r3` (SetErr, CopyPketData) are ret's defs: their r3 ghosts are NOT adjacent
  to ret (old value dead at the def). The IsZero `unit` r3 / `p` r4 order is a separate L1 vid-order residue (CSE temp @701 below the clone param
  @681); unchanged by every probe.
- **adx_sje write_end_code 2w, read with the 2.6 dump + blkflags (passes 06-13 here: 06 pre-RA schedule, 09 regalloc, 10 CSE = the sth->lha
  forwarding, 13 post-RA schedule):** site 2's store block B17 is dirtied at 10 (`lha r0,v` -> `extsh r0,r31`, n survives the call in r31) and
  rescheduled at 13 to `lwz r6; extsh; mr r3; addi; sth; li` = the 2.7 production bytes of BOTH ours and the target (so 2.7's post-RA scheduler ==
  2.6's on a dirty block). Site 1's block B12 is never dirtied in 2.6 (000c through 13, final = pre-RA `lha; mr r3; lwz r6`), but the 2.7 production
  gives `mr; lwz; lha` (ours) / `lwz; mr; lha` (target): 2.7 either reschedules B12 or its pre-RA order differs — the same cycle assignment in both
  ([mr|lwz], [lha, addi], [li, sth]), only the cycle-0 LSU/ALU order differs; the target's LSU-first matches B17's `lwz; extsh; mr` pattern. Spellings
  that leave 2w: an inlined store helper with (value, dst) or (dst, value) parameter order (initial code still `lha; lwz; sth`, B12 flags unchanged),
  a 2-byte struct copy `*(S16 *)ck.data = *(S16 *)src` (same bytes, lha kept). `#pragma scheduling off` on the function 57w. No 2.7 dump exists
  (mwcc_debugger has GC/1.1 and GC/2.6 offsets only); the open question is what 2.7 does to B12 that 2.6 does not.
- Tree untouched (sfd_mps 25/26 with the M1 pin, adx_sje 16/17); objects.py untouched; no flip, no `ninja -k 0` needed. Harness deleted.

### MWCC pre-RA scheduler (RE)
(2026-09-12, read off GC/2.6 mwcceppc.exe with `objdump -d -M intel` (`~/.cache/mwsched/dis.sh START END` = one routine without
byte columns); located from the debugger's "after-scheduling" breakpoint 0x433E0C = the instruction after `push 0; call 0x507c70`
(post-RA: `push 1; call 0x507c70` at 0x43405c). Raw-input dumper `~/.cache/mwsched/dump.sh lib/unit Func` (gdb script over
retrowin32's stub; writes `out_Func/sched-{pre1,post1,pre2,post2}.txt` = every block's pcodes with ADDRESS, flags word, operands
`R<class>:<reg>:<rw>` and the alias record) and the model `tools/research/mwccdbg/sched.py DUMPDIR [--post]` (README entry).)
- **Entry 0x507c70(postRA):** picks the machine model from the `-proc` byte 0x5eb096 (gekko = 4 -> table 0x5d5b50; 750 = 3/6 ->
  0x5d5028; 7400/7450 -> 0x5d4498/0x5d1c48; generic -> 0x5d3970) and walks the blocks: scheduled iff `pcodeCount > 2 &&
  !(flags & 8) && (postRA || !(flags & 3))`, then `flags |= 8`. Post-RA is the SAME routine; the only differences are the block
  gate and that the pre-RA-only tie-break (below) is off because 0x5ea638 ("virtual registers in use", set at codegen start
  0x4fe688, cleared after the RA at 0x50874d) is 0.
- **Per block 0x507d80:** nodes are built walking the pcodes BACKWARDS (block+0x18 = last pcode, pcode+4 = prev); the DAG
  builder 0x508090 sees only later pcodes, so every edge goes earlier -> later. Node: lat = model.latency(pcode) (0x57a830:
  table byte 1, +2 if `flags & 0x20000000 && !(flags & 9)`, + argc-2 for lmw/stmw), height (init lat), npreds, ready, deadline.
  Edges (0x508480; a duplicate edge only raises the latency and is not counted again):
  * register operands (kind 0) in operand order; GPR r2, r13 and an r0 with no rw bits (indexed base) are skipped; `rw & 2` =
    write (a read/write operand counts as a write only). Write -> kind-1 edge to every LATER reader and every later writer of the
    (class, reg); read -> kind-1 edge to every later writer. The reader/writer lists are never cut at a redefinition (edges are
    transitively redundant but they COUNT in npreds and in the "frees" criterion).
  * memory: load (`flags & 0x20002`) -> kind-1 edge to every later store that may alias; store (`0x40004`) -> kind-1 edge to
    every later load and later store that may alias (`flags & 0x40000` stores also enter the load list). Loads never depend on
    loads. May-alias (0x511ce0 on the pcode+0x18 records, type byte +0x2c): t0 = whole object (indexed access), t1 = object +
    off + size, t2 = pointer access with an alias-class bitset. t0/t0: same record; t0|t1 vs t0|t1: same object (or same link
    name), t1/t1 additionally the byte ranges overlap; obj vs t2: the object's index bit is in the set; t2/t2: same record or
    the sets intersect. Volatile/type play no role beyond that.
  * `flags & 0x80`: kind-0 chain in original order among themselves.
  * barrier = `flags & 0x1000000` (bl/bctrl) or `flags & 0x100` (dcbt) or table byte 5 (every branch, mtctr/mtlr/mtcrf/mfspr/
    mflr/mfcr/sync/isync): kind-0 edge to EVERY later pcode, and every pcode gets a kind-0 edge to every later barrier.
  * a node with no successor gets a kind-0 edge to the block's `flags & 1` terminator.
  * kind-1 latency = lat(source) (for the gekko table WAR/WAW carry it too: model word +4 = 1; word +8 = 0 extra into a branch);
    kind-0 = 0. height = max(lat, edge lat + succ height); after the walk deadline = maxheight - height.
- **List scheduling (0x507e5c):** cycle 0,1,2..; per cycle up to model.width (= 2) picks; candidate walk 0x507f50 over the
  unscheduled pcodes in ORIGINAL order, a candidate needs npreds == 0, ready <= cycle and model.canIssue(). best = the first
  candidate; a later one replaces it iff, in this order: (1) it is "urgent" (deadline <= cycle) and best is not (best urgent,
  cand not -> keep); (2) it frees MORE successors (count of successor edges whose target has npreds == 1); (3) its height is
  GREATER; (4) pre-RA only: its opcodeinfo byte +9 (0x5c0fa8 + op*0x12 + 9) is LOWER (0 = branches/mr/nop/fmr, 1 = stores/
  cmp/mtctr/mtlr/dcb*, 2 = int arithmetic, 3 = loads, 4 = li/lis/mf*); (5) otherwise keep = the earlier pcode. On issue:
  succ.npreds--, succ.ready = max(succ.ready, cycle + lat); model.issue(); the pcode is appended to the (emptied) block.
- **Gekko machine model (0x5d5b50; per-opcode 6 bytes at 0x5d5b78 + op*6 = unit, latency, occ1, occ2, occ3, barrier):**
  units 0 BPU, 1 IU1, 2 = "IU1 or IU2" (IU1 preferred), 3 -> 4 LSU stages, 5 -> 6 -> 7 FPU stages, 8 SRU. Rows: branches
  0/0/0 barrier; loads+stores 3, lat 2, occ 1/1; dcbz lat 3, occ 1/2; simple int (add/addi/rlwinm/li/lis/mr/and/or/extsb/
  cntlzw/srawi/subf/neg/nop) class 2 lat 1; cmp/cmpi/cmpl/cmpli class 2 lat 3 (occ 1); mullw/mulhw class 1 lat 5 occ 5,
  mulli 3/3, divw 19/19; mtctr/mtlr SRU lat 2 occ 2 barrier, mfspr/mflr/mfcr/mtspr SRU 1/1 barrier, sync 3/3, isync 2/2;
  FP fadd/fsub/fmr/fneg/fabs/frsp/fctiw/fcmpu/ps_* unit 5 lat 3 occ 1/1/1, fmul/fmadd lat 4 occ 2/1/1, fdiv 31/31;
  lwarx/stwcx lat 1. State: 9 unit slots (pcode, busy), a 6-entry in-order completion queue (issue needs a free entry; at most
  2 retire per cycle, only when the head is done), last1/last2 = the ops that completed in IU1/IU2 at the end of the previous
  cycle. canIssue (0x57a680): queue not full; class-2 op: at least one IU free, and if only one is free the op must not read or
  write the GPR written by the other IU's occupant nor by last1/last2 (0x507bf0: first operand of the other = a GPR write);
  other classes: the unit slot must be empty; a store cannot issue while LSU stage 2 holds a store (so stores issue at most
  every other cycle, loads one per cycle). advance (0x57a260): busy--, retire <= 2, then IU1 / LSU2 / FPU3 / SRU / BPU / IU2
  slots with busy 0 complete (fdiv/fdivs complete from FPU1), FPU2->3, FPU1->2, LSU1->2 move when the next stage is empty
  (busy = occ2/occ3).
- **First validation (sched.py, pre-RA dumps of the three functions):** sfd_cre AnalyMpv 8/8 blocks identical; mpv_umc
  OneReadMb 2/3 (B1 diverges at slot 38 of 72); mpv_mcy 4p 1/3 (B4 diverges at slot 4). Debugging continues below.

### CRI SWAR kernels pass 11: sched.py as the oracle — mpv_mcy 16x16 4p 122 -> 119w APPLIED (pair loads + masked loads: target block split and target pre-RA order of block 1; residue = colours, CSP-consistent), 8x8 4p read (72w unchanged); nothing flipped (2026-09-12)
Harness ~/.cache/cri_swar11/ (`gen.py NAME lpos=top|pair|pair2 lord=ab|ba p8=pre|mid cast=1|0 pix=.. mask=0|1 decl=..` whole-function
generator for the 4p body, `try.sh unit NAME FUNC` = variant.sh + scheddump + sched.py + dtk listing, `score.sh NAME` = the first-half block isolated
(`whatif.py DUMP BLOCK --keep a-b`: sched.py on an edited block) then `post.py TARGET_LST FROM TO ORDER --valid`: builds the TARGET's post-RA DAG
from its asm (physical registers, same alias record everywhere) and schedules it with the model using OUR pre-RA output as the input order;
`--ties` lists the input-order ties that decided the target's own schedule; `sym.py`/`tsym.py`/`cmp.py` = symbolic listings (a_k = s0[k], b_k = s1[k])).
- **Method result: the target's final block-1 order is reproduced (67/67, dataflow-valid) by the post-RA model from OUR pre-RA output when the
  pixel pair k+1 is loaded right before the sum p_k (`a1 = s0[1]; b1 = s1[1]; p0 = ..; a2 = ..; b2 = ..; p1 = ..`; gen `lpos=pair`, also `pair2`,
  `lord=ba`).** With the tree's all-loads-first form the model diverges at position 42 (`lbz a8` before `lbz b7`: a8/b8 sit at the top of the
  raw order, an input-order tie) and the target's colours are dataflow-invalid for 2 instructions there (so the vendor's pre-RA order differs
  around p6/p7). Fresh `a9/b9` names (65/67), `cast=0` (32/67), `s0/s1 += stride` before the sums (25/67) are worse.
- The post-RA model with the target's own order as input is a fixed point (67/67); its input-order ties: `b1 < a2 < b0 < a1` (loads), `b5 <
  a6 < a4`, `b4 < a5 < b6`, `add p3 < add p0`, `rlwinm p5 < add (a8+b7)+b8`, `add p7 < stw d[0]`.
- `lpos=pair` + masked loads (`s0[k] & 0xFF`, pass 9's E) splits the body exactly after `d[1]` (B3 = 79 pcodes, B4 = 69), prologue identical,
  119w; the residue of block 1 is now the colours only (same operation multiset in the same pre-RA order).

### CRI pass 49 (sfd_cre AnalyMpv 15w / adx_tsvr nlp_trap_entry 2w / mpv_umc OneReadMb 48w / sfh_main SmpHz 6w read with sched.py; IN PROGRESS; 2026-09-12)
Harness ~/.cache/cri49/ (`mk.py BASE OUT FUNC_START FUNC_END 'OLD=>NEW'..` = literal edits inside one function, `try.sh UNIT ABS_SRC FUNC`
= variant.sh words + differing lines, `whatif.py DUMPDIR BLOCK [ins=i:Pline] [reg=i:arg:cls:reg:rw] [raw=..] [del=i]` = sched.py on one block with DAG edits;
deleted at the end).
- **sfd_cre `sfcre_AnalyMpv` 15w: the target's two requirements, both now exact.** (1) RA side (chaitin.py on the v1 dump): the target colours
  (b4 r4, b7 r5, ofs r6, b5 r6, b6 r7, b8 r8, ofs+1 r0) are reproduced by removing ONE edge, ofs+1 (r49) -- rlwinm temp (r51), PROVIDED b7 is an
  own local with vid between b4 and ofs. In the tree b7 is forward-substituted into the `if` (same block, single def) as @164 (vid above every own
  local): removing the edge there gives @164 r4 / b4 r6, wrong. `Uint8 b4; Uint8 b7; Sint32 ofs; ..` + a second def `b7 &= 0xF; if (b7 < 1 || b7 > 8)
  .. sfcre_mpv_picrate[b7]` (no picrate_code local) keeps b7 an own local (`lbz r42` at its statement), bytes unchanged (15w) -- keep this shape.
  (2) Scheduler side (sched.py, block B6 = 16 pcodes, 2-wide, 9 cycles = 8 loads): ours c0 `lbz b7 | subf ofs`, c1 `add data | lbz b4` (the
  `addi ofs+1` is a candidate at c1 but is blocked by the gekko forwarding rule: only one IU free and it reads r42 written by the op that completed
  in IU1 at the end of c0), c2 `rlwinm | addi ofs+1` (frees 1 > a load's 0), c3 `cmpi | lbz b5` -> ofs+1 lives across the rlwinm temp = the edge.
  The target needs the addi AFTER the cmpi (c3+) and `lbz b5` after the addi. Raw order cannot do it (frees decides c2); the DAG must change. The
  model's single-edit hits: (a) an extra int op of class <= 2 (a `mr`) ready at c1 with frees 1 that reads `data` or `ofs` -- it wins c1's first IU
  pick (class 0 beats add's 2 on the frees/height tie), the `add` is then forwarding-blocked (one IU left, reads r42) -> c1 `mr | lbz b4`, c2 `rlwinm |
  add`, c3 `cmpi | addi`, c4 `lbz b5 | subf size` = the target colours; the copy must be coalesced/deleted by the RA (the block is dirty anyway:
  record-form merge). Forms: `mr @t, data` with no in-block consumer (WAR to the add), or `mr @t, ofs` consumed by the `addi` (`ofs + 1` from a
  copy of ofs). (b) the `subf ofs` delayed to c1 (a copy of p or data that only the subf reads): add and addi both forwarding-blocked at c2 -> c2
  `rlwinm | lbz`, c3 `cmpi | add`, c4 `addi | lbz b5`; needs the raw load order b6 before b5 (else `lbz b5` takes c2 and interferes with ofs).
  The post-RA model on the hypothesised pre-RA order with the target's colours reproduces the target's final order exactly (h1/ directory).
  C probes that did NOT create such a copy (15w, same object): `Sint8 *p` + `q = (Uint8 *)p` for every use (propagated), `Uint32 ofs`,
  `(Sint32)((Uint32)ofs + 1)`, `(Sint32)(p - data)`, b6 before b5, `data = data + ofs` / `data += ofs` / `ofs + data` pointer forms (all give
  `add r29,r29,r6`: only the INTEGER form `(Uint32)ofs + (Uint32)data` keeps the source operand order the target has; the frontend preserves int
  operand order and canonicalises pointer+int to pointer first), `data = data + ofs + 1` (reassociated to `data + (ofs+1)` and CSE'd with size's
  temp, 26w), `size = size - ofs - 1` (reassociated to `size - (ofs + 1)`, same bytes). Open: the C that leaves a coalescable copy of `data` or
  `ofs` in B6 (frontend copy sources known so far: `?:` results, call results with >= 2 uses, range-split IV copies, kept parameter copies).

### CRI pass 51 (adx_sje write_end_code 2 -> 0 pure C, unit 16 -> 17/17 FLIPPED, 111 OK; mwsfdcre CalcWorkSfd 4w: the epilogue hoist is the RA's deleted `mr r3, size` dirtying B15, the target's `add r3, sib, size` is computed straight into r3; sfd_mps DecodeOneUnit 13w: the model closes with ret adjacent to its own two def ghosts; 2026-09-12)
Harness ~/.cache/cri51/ (`sje/try.sh NAME 'stmt'`, `cws/try.sh NAME 'tail'`, `cws/try2.sh NAME 'chain'`, `mps/whatif.py DUMP a:b..` = chaitin replay with added edges; deleted at the end).
- **adx_sje `adxsje_write_end_code` 2 -> 0, pure C (APPLIED, FLIPPED).** sched.py on the 2.6 dump (17/17 blocks identical to ours) shows site 1's block B12
  input `lha v; lwz ck.data; sth; mr r3; li; addi; lwz; lwz; mtctr; bctrl`; LHA and LWZ tie on every criterion (both h=10, dl=0 urgent, one successor STH
  with npreds 2, class 3), so the earlier pcode wins: the target's `lwz r6; mr r3; lha r0` needs the ck.data load BEFORE the value load in the input.
  The frontend evaluates an EASS's RHS first and forward-substitutes a single-use pointer local (`Sint16 *dst = ck.data; *dst = *src;` = same tree,
  `register`/split declaration/volatile dst/`((Sint16 *)ck.data)[0]`/index local all 2w; `volatile` on the SOURCE keeps site 2's lha, 7w; memcpy = a
  real call). A two-use pointer survives: `Sint16 *dp = (Sint16 *)ck.data; *dp++ = *(Sint16 *)src;` (the dead `addi dp,dp,2` is deleted, bytes
  IDENTICAL; `*dp = ..; dp++;` as two statements and `dp = dp` are substituted again). Not a 2.7-vs-2.6 difference: pass 48's "2.7 reschedules B12" was
  wrong, both compilers give the same order from the same input.
- **mwsfdcre `mwPlyCalcWorkSfd` 4w, the two words are ONE event.** blkflags: B15 (the chain block) has its scheduled bit cleared at pass 09 (regalloc)
  because the RA deletes the coalesced `mr r3, size` of `return size;`; pass 11 merges the 1-pred return block's `lmw; lwz r0; mtlr` into B15 and pass 12
  (post-RA scheduler) then hoists `lwz r0,0x34(r1)` above the last `add`. The target's `add r3, r29, r3; lmw; lwz r0` = the merged block NOT rescheduled
  = no copy deleted in it = the return value computed straight into r3 as `sibsiz + size` (sib first). So the source is `return sibsiz + size;` with
  `size` NOT forward-substituted. Negatives (13w = the whole chain substituted into the return and reassociated by the backend): `{ return .. }`,
  `if (0) size = 0;`, a dead `size2 = size;`, `+ 0 * size`, `(size2 = size)`, `(size2 = size, size2)` (9w), `*&size`, `(Sint32)(Uint32)size`, `(Sint16)`
  (10w, +4 bytes), `Uint32 size` with casts, an inlined `mwsfcre_addsiz(sibsiz, size)`, an inlined empty call / empty call taking `size` between,
  the whole chain in an inlined helper `return sibsiz + helper(..)` or `size = helper(..); return sibsiz + size;`, `asm { }` between (13w; `asm { nop }`
  53w). A `+=` chain on ONE variable ending `size2 += adxwksiz; += consts; return sibsiz + size2` is merged whole (the constants land on a leaf). Rule
  read off frontend-01: consecutive constant `+=` merge into one; an rvalue single-use read pulls the def chain in whatever the distance/casts/blocks;
  `x += var` statements are kept (lvalue). Still open: a codeless second read of `size`, or the reason the vendor's `size` had two uses.
- **sfd_mps `sfmps_DecodeOneUnit` 13w (pin) / 132w (n2 = no pin + `?:` n):** whatif on the n2 dump: adding ONLY the two edges ret–r89 (the SetErr `mr @t,r3`
  ghost) and ret–r153 (the CopyPketData `mr @t,r3` ghost) reproduces the target colouring exactly (ret r31, wk r30, nskip r29, nbyte r28, len r27, data
  r26, sfd r25, delim/p r24, mps r23, total r22, bufin/dst/cnt r21). Those two ghosts are the only ghosts of the function NOT adjacent to ret (ret's
  old value is dead at its own defs); every other ghost and all of r0/r1/r3..r12 already are. So the vendor's ret was live at its own redefinitions
  (a read-modify def or a conditional def) or two new ghosts sit in a ret-live region. Negatives: a user copy of `sfd`/`data` at a call (`s = sfd;
  f(s)`, in a helper or inline) is forward-substituted by the frontend (no ghost, bytes unchanged); a neighbour pin `asm { mr r11/r8, ret; mr ret, rX }`
  after `ret = 0` is 134/135w (r11 is used once in the target; the r8 form is no better) — the codeless-pin row does not hold for a constant-defined
  own local here. Tree keeps the M1 pin (13w).

### CRI pass 52 (adx_dcd5 Ste4AsSte 25w / Ste4AsMono 143w, sfd_mps DecodeOneUnit 13w unchanged: a `?:` on a parameter IS a coalesced ghost pair but always keeps its compare; the pass-47 ghost model made exact (`phys=` adjacency, 18/18); nothing applied, nothing flipped; part 2 below; 2026-09-12)
Harness ~/.cache/cri52/ (`mk.py OUT [--base=B] 'OLD=>NEW'..`, `try.sh NAME FUNC --ra` = variant.sh words + ra.py into ra_NAME + `ghosts.py`
(named colours + ghost list), `whatif.py RA_DIR +ghost:REG:phys=REG.. --target name=reg,..` = chaitin replay with extra never-removed ghosts; deleted at the end).
- **The pass-47 "18/18" reproduced and made precise.** a7 rebuilt from the pass-47 text (114w, chaitin IDENTICAL). The ghost model that scores 18/18 is a ghost
  aliased to physical rP whose neighbours are exactly the nodes that already interfere with rP (`phys=`; a ghost adjacent to every node scores 3/18, so the
  pass-47 wording "adds +1 to every loop node" is wrong for histl: r6/r8 already interfere with 71 of 76 nodes because the final `histl[0] = l1` stores keep them
  live, and the ghost doubles THAT set). Enumerated multiplicities (0..2 ghosts on r6/r8/r9/r10): 18/18 iff (#r6 + #r8 == 2) and (#r9 + #r10 >= 1); the
  histl pair alone (2,0,0,0) is 15/18 (s/nblk/t = r20/r21/r19 instead of r21/r19/r20), a pair on both hist pointers (2,2,..) is 5/18 (qtbl jumps to r9, c1e r31).
- **A `?:` on a parameter makes the pair, and its compare survives:** `hl = histl ? histl : histl;` / `(nfrm > 0) ? histl : histl` / `nblk ? histl : histl`
  (loads and final stores through hl) -> `r35->r6=histl` + `r71->r6=@58` in the compiler's ghost list, the colouring = the model's (2,0,0,0) (c1e r9, c2e r10,
  l2 r31 .. qtbl r22 right, s/nblk/t off), but the backend keeps the condition's `cmplwi r6,0` (branch and both `mr @58,r35` arms deleted, size +4, 137w).
  Frontend AST: ECOND with EINDIRECT histl in both arms; backend-00: `cmpli; bt; B3: mr r71,r35; b; B4: mr r71,r35` = a two-def copy web, which is why r35 is
  not propagated (the pass-47 rule "source of another mr" = "source of a mr the copy propagation cannot fold" = a multi-def destination).
- **Frontend-folded (no ghost, 114w unchanged):** `1 ? histl : histr`, `(histl == histl) ? ..`, `(sizeof(Sint16) == 2) ? ..`, `zero = 0; zero ? histr : histl`,
  `i = 0; hl = i ? histr : histl` (reaching-definition constant, even for the multi-def loop counter), an inlined `static pick(a, b, sel) { return sel ? b : a; }`
  called with `sel = 0`, `register Sint16 *hl = histl`, a dead second def `hl = histl/histr/NULL` in the early-return arm (dead-store-eliminated in the AST).
  So the frontend folds every `?:` whose condition it can evaluate, and the backend deletes the select but never the compare: **a `?:` on a parameter is a
  ghost pair at the price of exactly one compare instruction**; pass 48's `n` was free only because its compare replaced the `if`'s.

### CRI pass 50 (cftfx StaticV 36 -> 16w and Argb420 38 -> 24w in pure C: `const` parameters = loads with their own alias class (hoisted above the prologue), the increment order that the addi sink leaves alone; UserTable 135w unchanged (the in-loop `subi` + 2 copies not found); cftyp422 Y84C44 29 -> 28w with `const src` (not applied: sfx.h prototype); nothing flipped; 2026-09-12)
Harness ~/.cache/cri50/ (v.py/ut.py/mac.py literal-edit variant drivers over tools/research/kit/variant.sh, sd_* scheddump dirs, ra_* ra.py dumps; deleted at the end).
- **NEW ALIAS LEVER (pure C): a load through a pointer-to-const gets its own alias record and never aliases a store.** Read off the
  scheddump records: `lwz r9,4(r4)` with `CFT_ARGBDST *dst` = `t2 bits=2` (the function's "unknown pointer" record, shared with EVERY
  load/store including the prologue's `stwu`/`stw`, flags 0x22); with `const CFT_ARGBDST *dst` = `t1 obj=@77 off=8388611 size=4`
  (a pseudo-object per const pointer, flags 0x42), which the may-alias test (obj vs bitset: idx not in {1}) declares disjoint from
  the t2 stores. `const Uint8 *tbl` gives the `lbzx` a `t0 obj=@96 size=0xFFFFFF` record likewise. A `const Uint8 *y = src->y`
  LOCAL does not (the class follows the pointer's origin = a load from a non-const struct, still t2). Consequence in the post-RA
  entry block: the parameter loads no longer depend on the frame store, so `lwz r9,4(r4); li r6,0; stwu; srawi; lwz r5,8(r4)..`
  with the two `stw r31/r30` saves scattered 10 and 20 slots later = sched.py on ours' B0 with the three stack stores given a
  disjoint record: IDENTICAL to the target's 23 slots (StaticV). Rule: when the target's prologue has parameter loads ABOVE `stwu`
  (or scattered callee-saved saves), the vendor's parameter was `const T *`; when `stwu; li; lwz; stmw` (UserTable, `lwz` waiting
  2 cycles) it was not. UserTable/Argb420's targets have non-const `src`/`dst` (tested: const src hoists `lwz 0xc(r3)` above stwu).
- **cse2 F6' fit = the LIFE knob, but it cannot be held:** variant /tmp/t20/s1_b.cpp (LIFE `1:2:3:5..60:4`, RELEASE/ANMRATE/ROTATE `{ }`)
  = 1933w, lc segs 221 545 681 698 699 782 (p20b: 545 555 619 621 623 624 625 663 690 782): every F6' symptom (619/621/623/624/625/
  663/690, and 5198's 555C) is fixed, F6' = ANMRATE+35 (target interval (FLAG+246, ANMRATE+45] in p20b .loop coordinates = 46..545
  insns earlier than p20b's ROTATE+21). Cost: the pad's 56 cse1 insns move F9 -48 (ROTATE+623), F10 WORK0+18 -> SUB+162 (the WORK0
  `DB_POINT pos` addressof pair is (WORK0+14, WORK0+33] and 525C's load at WORK0+9 must be cse1-fresh + cse2-rescued from 681 ->
  F10 in (WORK0+9, WORK0+33] EXACTLY as p20b has it), F11 WORK5+83, F12 BASEPOS+79, and F7' SUB+95 -> SUB+31 (F7' must not lie between
  681's and 698's loads). Removable cse1 pads between LIFE and WORK0+18: RELEASE/ANMRATE/ROTATE/VEC0/VEC1/VEC2/SUB/WORK0 = 32 (VEC0's
  costs 3 cse2), F10's slack 3 -> D <= 35 < 46 needed. So the target has >= 11 MORE cse2-time (and cse1-time) insns between F8 (LIFE+8)
  and F6' than any pad form gives, or fewer cse1 insns between F8 and F10: not a pad question any more. NOT applied; p20b stays.
- **F7 is misread by pass 19 (5104 at 545/547):** target 5104 (.LC1529) 543F then SHARED at 545 (jump COLOR+880) and 547 (+916); 50DC
  16.0 shared at 543 (+853) and fresh at 561 (FLAG+58). Hence F7 in (COLOR+916, FLAG+58] (cse1-shared 545/547), or F7 in (880, 916] with
  F5' outside (COLOR+557, COLOR+613] .loop (cse2 rescue of 547 from 543's load). p20b's F7 = COLOR+863 gives 545 fresh; a pointer-pair
  POS_MINMAX (`f32* mm_ = &max; mm_[0]=..; mm_[1]=..`, /tmp/t20/pp.cpp) removes 24 cse1 insns in POS/BASEPOS and puts F7 at +887 (545
  shared, 547 fresh) but loses the FSet alias effect (PATH region mset 3 -> 14) and 50DC 486 -> 498: 2133w, rejected. Between F4
  (certain: 284.0/.LC1581 at 376F 377 shared 378F -> a cse1 flush in (PATH+510, PATH+542]) and F7 the target therefore has >= 18 (pointer
  form) / >= 53 (FSet form) FEWER cse1-time insns than ours; PARENT's pad (4) is the only removable pad there, the `pa_ = pa; win_ =
  win;` block copies are not cse1 insns (removing them in POS: grid unchanged, 4058w — statement order), FSet costs exactly 1 cse1 insn
  per store (12 in POS, 6 in BASEPOS; direct stores 0). Open: which real construct in PATH-tail/PARENT/POS/SIZE/SPEED/COLOR-head has
  ~4-9 more RTL insns per CreateNumeric2 block in ours than in the original (candidates: the named `DB_POINT pos` local vs a
  `&DB_POINT()` temporary, `int sx` locals, `SetKeta` + POS_MINMAX order); measure with `wincount.py` on the `.jump` dump.
- **Per-variant harness /tmp/t20 (disposable):** `v.sh NAME PADS..` (p20b pad base), `vv.sh NAME` (any /tmp/t20/NAME.cpp: autoN +
  grids + words + mset + lc), `scan.sh`, `seg.py O_init.s SEG..` (side-by-side of segments), `hl.py` (symbol-high register timeline),
  rtl dumps rtl_p20b (.jump/.loop), rtl2_p20b (.cse2), rtl3_p20b (.lreg/.greg). /tmp/t18, /tmp/t19, ~/.cache/tesp15, the kit untouched.
- Not run: make_rel --verify, ninja -k 0, shasum (nothing flipped; flags untouched). Next: (1) find the per-block RTL surplus between F4
  and F7 (above) — it is the same kind of thing as pass 16's "+4 per window", with the opposite sign, and it unlocks F7 > 916, F5', and
  gives the LIFE knob its cse1 room; (2) then the LIFE knob for F6' (D = 46..60) with the cse1 held at F10 in (WORK0+9, +33];
  (3) the callee-saved/spill picture is reload's rotation over {r0,r6,r8,r9,r10,r11} + the callee-saved reload registers r14-r17:
  judge after the grid (the `li 1`/`lis` fresh-vs-inherited pattern follows from it).
- **APPLIED (src/lib/mpv_mcy.c `MPVMC16_OneRef4p_TuneC`, 122 -> 119w, size equal, pure C):** first half = `a0 = s0[0] & 0xFF; b0 = s1[0] & 0xFF;
  a1 = ..; b1 = ..; p0 = ..; a2; b2; p1 = ..; ... a8; b8; p7; a0 = s0[9] & 0xFF; b0 = ..; p8` (pair loads before each sum, mask at every load,
  `(Uint32)` casts kept, declaration order px, ps, lv unchanged); second half untouched. Prologue identical (stmw r21/-0x40, loop vars r0/r3-r6,
  `li r7,0x10`), body split B3 = 79 / B4 = 69 as the target, block-1 operation multiset in the target's pre-RA order. Not flipped (H2/V2 225w).
- **Residue of block 1 = colours only, and the colours are NOT a contradiction of the graph:** `csp.py` (greedy order search on m1's interference
  graph with the target's colours per value) finds a colouring order reproducing all 53 block-1 target colours once the callee-saved set r21-r31 is
  handed out before any block-1 node (the target hands r31..r21 to BLOCK-2 nodes first: p8 r31, p9 r30, p10 r29, p13 r28, b14 r27, b13 r26, a16 r25,
  b16 r24, b12 r23, b11 r22, a13 r21 — block-1 pixels then take the LOWEST free handed register: b5 r28, a5 r27, a4 r26, b6 r25, b8 r24, p1 r23,
  b2/b4/p0 r22, a3/b7/p7 r21). So block 2 is coloured before block 1 (higher vids, all L1) in the target as in ours, and the block-1 colouring order
  the target needs is: p8's chain temps, a7 r8, a8 r9, a9 r10, a6 r7, p6/p3/p2/p1 first adds, a2 r9, .., a3 r21 / b2 r22 late, p0/p5/p6/p7 own
  locals last. Ours colours block 1 in descending backend-temp creation order (pair form: p8 temps, b9, a9, p7 temps, b8, a8, ..., a1) = 6/49
  target colours; pixels as own locals in any of 8 declaration orders (chaitin what-if, `csearch.py`) <= 12/49. Open: the vid order (creation order of
  the sum temps vs the pixel loads) that gives the CSP order; not searched further (time box).
- **Block 2 (target lines 77-133) starts `lbz a10, lbz b10, add (a10+b9), lbz a11, add +b10, lbz b11, addi, add p9 = a9 + ..` = the same pair
  pattern; ours (all second-half loads first) is frontend-scrambled (p15's loads and sum first, then p11, p10, p9). A pair-form second half (q1: pixel
  10 loads, p1, pixel 11, p2, ...) compiles to the same 119w; its block-2 model score was not read (the cross-block values a9/b9/p8 need the `--lv`
  vreg mapping in sym.py; 5 minutes) — next pass: score it with `post.py t_4p16.txt 77 132 .. --tlv r4=i,r0=stride,r5=s0,r6=s1,r3=d,r12=b9,r10=a9,r31=p8`.
- **mpv_mc 8x8 4p (72w) read with the same tools (`score8.sh`, `gen8.py`):** ours = target for the first 4 loads, then the target issues `add (a1+b0)`
  BEFORE `lbz a2` at cycle 5 (both urgent, both free 1; our model gives the load by height 14 vs 11), and loads b5/a6 in ADDRESS order (positions 37/39)
  where ours hoists them to 13/14 (p5 = the d[1] pack base has the longer chain). Pair/pair2 load placement, `lord=ba`, dcbt after pixel 1 (68w but
  9/64 model), casts, loads-not-aliasing-stores, stores-not-aliasing (`whatif.py --noalias/--stnoalias/--nomem`) all leave the pick: the target's
  8x8 DAG has p1's chain no longer than p0's, i.e. its pack/store chain differs from ours in a way the alias records do not explain (the `or` ->
  `mr + rlwimi` pre-RA chain?). Unchanged at 72w. mpv_mc V2 73w / H2 436w, mpv_mcy H2/V2 225w not touched this pass.
- Tools kept in the harness (delete with it): `try.sh`, `score.sh`/`score8.sh`, `gen.py`/`gen8.py`, `batch.sh`/`batch8.sh`, `whatif.py`
  (sched.py on an edited block: `--keep`, `--order`, `--noalias`, `--stnoalias`, `--nomem`), `post.py` (target post-RA DAG + model + `--ties`,
  `--valid`), `sym.py`/`tsym.py`/`cmp.py`/`colmap.py`/`annot.py`/`csearch.py`/`csp.py`. Kit untouched. objects.py untouched by this pass.

### CRI pass 51, part 2 (sfd_tst SFTST_Calc 79w: the pre-RA order is not the cause; adx_baif AIFF_GetInfo 170w: five more 16-bit-hi spellings never emit the `clrlslwi`; mwsfdcre CreateSfd 115w: four more helper bodies negative; nothing else applied; 2026-09-12)
(Continuation of "CRI pass 51" above; another agent's pass 52 section landed between. Tree state at the end: adx_sje 17/17 FLIPPED (objects.py `# CRI pass 51` block, 111 OK); sfd_tst 10/11, sfd_mps 25/26 (M1 pin kept), adx_baif 5/6, mwsfdcre 8/10 unchanged; no other file touched. Harness ~/.cache/cri51 deleted.)
- **sfd_tst `SFTST_Calc` 79w, scheduler view:** the 79 words are ALL ARG_MISMATCH (no insert/delete); scheddump + sched.py on ours: 77/77 pre-RA and 37/37
  post-RA blocks identical to the model. The MulDiv/sprintf blocks (B84, B90-B92 of the 2.6 dump) have their scheduled bit cleared at regalloc (coalesced
  copies deleted) and are rescheduled post-RA; the target's final order in that region equals ours instruction for instruction, so there is no evidence
  of a different pre-RA input order (a different input would have to survive the post-RA reschedule unchanged in every tie). The residue stays a pure
  colouring question (pass 46's +9 on MulDiv / the frontend temp between @171 and @172). No source change.
- **adx_baif `AIFF_GetInfo` 170w, the `clrlslwi r12,r12,16,8` (size +8):** the target builds `*nch`/`*bps` as `(hi & 0xFFFF) << 8` with lo INSERTED
  (`rlwimi ..,0,24,31`), i.e. hi has 16-bit range knowledge and lo 8-bit, while exp/mant use lo as the base and insert hi (`rlwimi ..,8,16,23` = both
  8-bit). Negatives (size unchanged, mask never emitted): `((Uint16)p[1] & 0xFFFF) << 8 | p[0]`, `(Uint16)p[1] * 0x100`, an inlined `aiff_mk16(Uint16 hi,
  Uint8 lo)` / `(Uint16 hi, Uint16 lo)` / `aiff_le16(Uint8 *p)` with `Uint16 hi = p[1]; Uint8 lo = p[0];` (the clone locals are propagated and the lbz
  range wins). The 16-bit hi must come from something the frontend cannot see through (a real 16-bit load, a `char`-typed byte with the extsb folded, or
  a two-use Uint16 local); untested: `Sint8 *`/`char *` views of the byte with `-char signed`.
- **mwsfdcre `mwsfcre_CreateSfd` 115w, the dead `b`:** target tree at both sites = `cmpwi m,4; beq T; bge T; cmpwi m,2; bge F; b T; b T; F: li 0; b E;
  T: li 1` — the surviving `b T` is a block whose only content was removed after layout with the `beq` already threaded to T. New negatives (whole-unit
  words): `case 4: return TRUE; default: return TRUE;` 120w (+8), the `?:` chain 143w, `Bool ret = TRUE` + `case 4: ret = TRUE` + `default: ret = TRUE`
  124w, `case 4: return (Bool)(mode == 4)` 133w. Pass 7's constraint stands (case-4's def redundant with a dominating def that costs nothing at both
  sites; not a post-RA physical-r0 CSE either: neither site has r0 == 1 dominating the tree).
- **mwsfdcre `mwPlyCalcWorkSfd` 4w, more negatives:** a forward `goto total; total:` label, `do { size += 0; } while (0);`, `asm { mr size, size }` on a
  `register size` (13w, the substitution still happens), `asm { mr r0, size }` 91w, a duplicated return under `if (mwsfdcre_bufnum == 0)` 20w / as `?:` 17w.
- **StaticV 36 -> 16w (APPLIED, `const CFT_YCC420PLN *src, const CFT_ARGBDST *dst` in the prototype and definition).** The 16 left are RA colours in
  rows 1 and 4 of the tile, both read in chaitin.py (`ra_c1`, IDENTICAL to the compiler): (a) row 4's `t` halves are the frontend's sunk webs
  (the LAST TWO defs of a reused single-use variable are substituted into their uses whatever sits between def and use: h4 probe with a 9th
  `t = w >> 3; d[8] &= t` def keeps row-4 hi as @179 and sinks row-4 lo + the 9th; a store between def and use does NOT keep a web (h3: `u = lo;
  t = hi; d[0] &= t|K; d[1] &= u|K` sinks u's rows 3-4 too); dead `t = 0` / `t = t` / `w = t` / `t += 0` after row 4 are deleted before the
  web pass and change nothing). As backend temps the row-4 lo (r109) is coloured before the d[6] load (r100): lo r11, hi r12, d6 r31. Moving
  lo's vid below r100 in the model gives the target's row 4 exactly (d6 r12, hi/hiK/and r11, lo r31, d7 r12): the vendor's row-4 lo was a
  low-vid node (an own-local web or a frontend @temp defined before the d[6] statement). (b) row 1: t-lo1 (@183 = r43) is coloured before
  w1 (own local r35) and takes r30, w1 gets r31; the target has w1 coloured first (r30) — w's first web must rank above t's split webs (a
  range-split web of w, i.e. a def of w before the loop that survives, or w as a @temp). Not found in 45 min: `t` before `w` (16w), t/u
  pairs (h1/h2 50w: extra callee-saved; h3 20w: row 1 fixed, rows 3-4 broken), `for (t = ..)` as the counter (98w), t as the outer-loop
  step (58-101w), `t = ystep * 4; y -= t` (33w). Model-confirmed target shape: t-lo(4) low vid + w1 above t's webs.
- **Argb420 38 -> 24w (APPLIED, pure C): the loop-tail increment ORDER decides whether peephole-forward's addi sink dirties the block.** Pass 43
  read the mechanism (pre-RA order = target, the sink of `addi cb/cb2/cr,2` dirties the block, post-RA reschedule); the source lever is the
  statement order `y += ystep; a += ystep; cb++; cb2++; cr++; cr2++; y++; a++; y -= yback; a -= yback; j++; ar += 16; gb += 16;` (t2):
  blkflags on the tree (ra_argb2): the tail block B13 is STILL sunk-dirty (2004 at 12-15) and post-RA rescheduled (200c at 16) — the sink
  moves `addi cb/cb2/cr/cr2,2` to the block end in both versions; what changed is their ORDER there (backend-12: `add y; add a; addi y,4;
  subf y; addi a,4; subf a; addi cb; cb2; cr; cr2; j; ar; gb`), and the post-RA list scheduler fills the two `subf` latency slots with the
  sunk increments in that input order -> `addi y; addi cb; addi a; addi cb2; subf y; addi cr; subf a; addi cr2; j; ar; gb` = the target.
  With the tree's order the sunk group was cr2/j/ar/gb first. Increments first (t1) 38w, folded `y += ystep - yback + 1` 35w, cb/cr before
  cb2/cr2 + j last (t4) 30w. Correction to pass 43: the target's tail IS a post-RA schedule (of the vendor's sunk order), not the pre-RA one. Left 24w = the prologue's y/cb copies (pass 43's reading (1) stands): `a = pln.y; y = a; a += half`
  forms (a1-a4) coalesce `y = a` anyway (58-61w, the split web of `a` is a @temp and the copy is forwarded).
- **UserTable 135w unchanged; the target's body read in full (registers: r5 tbl, r6 y, r12 d, r11 ywidth, r0 ywidth*4, r30 yskip, r3 dskip,
  r8 i, r9 wblk, r10 hblk):** per block `subi r31 = ywidth-4` IN the loop, `mr r7,r31; mr r4,r31` copies, then per row `addi p,p,4` (the
  macro's `y += 4`) and `add pNEXT, p, w4copy` into the copy's register (p2 = r31, p3 = r7, p4 = r4), the 4th step recomputed `subi r26,r11,4;
  add r26,r4,r26; subf r4,r0,r26; addi r6,r4,4` = `y += ywidth-4; y -= ywidth*4; y += 4` as three statements. The colours say the two copies
  are OWN LOCALS coloured before yskip (r4/r7 taken, yskip -> r30, w4 -> r31): declaration order roughly `copy2, y, copy1, i, wblk, hblk,
  ywidth, d, w4, yskip`. What no form gave: the def in the loop. Probes (all 135-137w unless noted): `const Uint8 *tbl` / `const Uint8 *y`
  local (no effect: the lbz class follows the pointer's origin), `src->ywidth - 4` in the body (reloads per row, 0x224), + `const src` (hoisted
  again, and the prologue loads jump above `stwu` — so the vendor's src/dst are NOT const here), per-row `w4 = ywidth - 4; y += w4` with a 5th
  def `w4 = ywidth * 4; y -= w4` (k8b: size 0x22c exact, webs 1-3 survive, the backend's pass-05 code motion hoists them into ONE preheader
  `subi`, 137w), three own locals `w4a = ywidth-4; w4b = w4a; w4c = w4a` (k11 137w, hoisted + propagated), block-scope `{ Sint32 o = ywidth-4; }`
  per row (k10), `Uint32 y` integer address (k14: identical object to the tree). Open: what keeps `subi` in the loop — in the target its
  register is redefined by the pointer add (`add r31,r6,r31`), so the vendor's step and next-row pointer may be ONE variable (`p = ywidth - 4;
  ... p += (Uint32)y + 4`-like integer forms) whose second def blocks the backend hoist; not tried (box over).
- **cftyp422 Y84C44 29 -> 28w with `const CFT_YCC420PLN *src` (y1; NOT applied: the prototype lives in src/lib/cri/sfx.h, another agent's
  header).** The target's `stwu; li r0,8; lwz r9,0xc(r3); li r8,0; stmw` has the ywidth load in cycle 1 = not dependent on the frame store =
  const src (same lever as StaticV). The other 28 words: `li r0,8` (= `ofs = 8`, the dcbz index) is slot 1 in the target and slot 10 in ours
  because ours' B2 (the setup, flags 0004 = not the entry block) is PRE-RA scheduled with vregs: `li r68,8` has no successor in the block
  (height 1, class 4) and sinks to the end; the RA then hands r0 to the setup's srawi/addze temps (ofs is not live there). In the target no
  setup temp uses r0 and the li is first: r0 was live/reserved from the block top — either the setup was in an unscheduled block (entry
  block) or r0 is a physical register named in the asm (`li r0, 8` written in asm, reserved function-wide per pass 42/44). `asm { li ofs, 8 }`
  at the top (y6) and `register Sint32 ofs = 8` (y5) are ordinary pcodes to the scheduler: still slot 10. Pass 45's residues (1)/(2) untouched.
- Flags: none flipped (cftfx 3/6 = 16 + 24 + 135w, cftyp422_ppc 7/8 = 29w). Tree edits: src/lib/cftfx.c only (StaticV const prototype +
  definition and comment, Argb420 tail order + comment). Locked `ninja build/G4BE08/src/lib/cftfx.o` after each; .rodata/.data/.bss unchanged.
- Catalogue additions (MWCC table): (row "instruction order within a block") a load through `const T *` never aliases a store — the target's
  parameter loads above `stwu` / scattered `stw` saves / a struct-field load hoisted over an earlier store = the vendor's `const` parameter;
  (row "loop body rescheduled post-RA") when the block is sunk-dirty in the target too, the residue is the ORDER of the sunk increments at the
  block end = their statement order; the post-RA scheduler uses them as latency fillers in that order (Argb420 t2: statement order is the lever).
### CRI pass 52, part 2 (continuation of "CRI pass 52" above; nothing applied, nothing flipped; 2026-09-12)
- **Why no `?:` can be free in Ste4AsSte / Ste4AsMono:** the target prologue has no compare at all (srwi/extsh/extsh/stmw/add/lis/lwz/lha/srawi/lha/addi/li/
  4 lha/b), and the only compares in the function are the loop's `rlwinm.` s-tests, the clamps and the bottom `cmpw i,nblk` — none precedes the hist loads.
  A two-def copy web needs a join; every join without a compare that C can write here (loop-carried `hl = histl` in the body: the `mr` stays because r71 and
  r35 overlap inside the loop; a dead second def before `return`: removed by the AST optimizer) is either code or folded. Both Ste functions therefore stay at
  the tree's pin forms (25w / 143w). Model note for Mono: the same `phys=` ghost reading applies (pass 47's "two ghosts on r6/r8" = the histl/histr copies).
- **DecodeOneUnit (13w), the requirement re-derived on the n2 graph with `ghostwhatif.py`:** two more never-removed nodes adjacent to {ret, data, wk} (or
  three adjacent to ret alone) give the whole target colouring (ret r31, wk r30, nskip r29, nbyte r28, len r27, data r26, sfd r25, mps r23, total r22, cnt r21,
  13/13); nothing else moves. The three ghost kinds of this function (pass 48) were re-checked against the target bytes in the cnt-dead region: (1) `?:`
  argument locals — `n` is the only if/else whose value reaches an argument; the `hdrlen > 0` arms store from both arms in the target (`lwz r0; stw; lwz r0;
  stw | li r0,1; stw; stw`, four stores), so they were not a `?:`; TermIfInTerm's `t` is already a `?:` in r3; (2) 2-use call results — the region's calls
  (IsEndcodeSkip 872, IsSystemEndcodeSkip 879, SetTermFlg/GetTermFlg of the inlined TermIfInTerm, CopyPketData = ret's def, MEM_Copy) are all single-use in the
  target's code shape; (3) own locals dying at an argument move — the target's PKET block is `add r26,r26,r0; subf r27,r0,r27; mr r4,r26; mr r5,r27` (in-place
  `data += hdrlen; len -= hdrlen;` on the callee-saved webs, not temps). So the +2 is not a `?:`: it must be a shape that changes which values are coalesced
  copies without changing the bytes — still open. The pin form stays (13w).
- **The separate IsZero residue (unit r3 / p r4, 8 of the 13 words) read exactly:** unit is the CSE temp r57 (@701), the clone parameter p' is r61 (@681); both
  L1 (11/12 and 15/15), so p' (higher vid) is coloured first and takes r3. The target colours unit first, i.e. unit had a higher vid than p' — impossible for an
  expression-CSE temp (created after the inlining: @681 < @701) — so the vendor's `unit` was not `sfd->prm.unit` CSE'd across the three uses but a value
  created before IsZero's clone (a local/@temp of an earlier inlined helper, or IsZero's own n if p' were not a clone copy). Negative (13w unchanged):
  `sfmps_IsZero((Sint8 *)data, ..)`, `p = data` moved below the test, `(Sint32)` cast on the count, IsZero parameters swapped `(n, p)`; `p[i]` index form 17w,
  `while (n-- > 0)` 56w (+4 bytes). IsZero is called only from DecodeOneUnit (its body belongs to the pass-51 owner: not edited).
- Kit: `tools/research/mwccdbg/ghostwhatif.py` (README entry). Tree untouched (adx_dcd5 2/4, sfd_mps 25/26), objects.py untouched by this pass, no flip, no
  `ninja -k 0`. Harness ~/.cache/cri52 deleted.

### CRI pass 49b (continuation of pass 49: adx_tsvr nlp_trap_entry 2w, mpv_umc OneReadMb 48w, sfh_main SmpHz 6w read with sched.py; nothing applied, nothing flipped; 2026-09-12)
Harness ~/.cache/cri49/ deleted; the one-block what-if driver is now in the kit: **`tools/research/mwccdbg/schedwhatif.py DUMPDIR BLOCK
[mv=i:j] [del=i] [ins=i:P ..] [reg=i:arg:cls:reg:rw]`** (README entry). sfd_cre's reading is in "CRI pass 49" above (the tree keeps the
pass-31 form; the b7-own-local two-def shape `b7 &= 0xF` is a zero-cost prerequisite of the target colouring, not applied because it is
15w alone). Tree untouched, objects.py untouched, no `ninja -k 0` needed.
- **adx_tsvr `adxt_nlp_trap_entry` 2w: the join block's pre-RA order is NOT the mechanism.** scheddump B16 = `lha ofst; add ofst1; lha ofst2v;
  cmpi n1; bt` raw -> pre-RA `lha ofst | cmpi` c0, `lha ofst2v` c1, `add` c2, `bt` c3 = the target's final order exactly; blkflags 000c through
  the post-RA pass (never dirtied, never rescheduled), so the vendor's block had the same order and the same live ranges. The lha temp's
  neighbours are r1, r3 (n2's @ret ghost `mr r55,r3; mr r54,r55` in the else-arm tail B15), ofst2v r26, ofst1 r27, sji/sjd/p, n1 r28: r0 is
  free in every order of these five pcodes (the only other values live in B16 are live-through). The target's r4 still needs an r0 node in
  B16; the BL's physical `R4:0:2` write is the only r0 def nearby and the temp is defined after it. Left 2w (pass 46's pin result stands).
- **mpv_umc `mpvumc_OneReadMb` 48w: the alias side is settled, the slot is a c33 tie.** (1) Alias records: every pointer load/store of the
  function shares ONE t2 record (`bits=0x1fa`); the two table loads are `t0 obj=mpvumc_oneref_y idx=3` / `obj=mpvumc_oneref idx=5` and alias
  the `mc->` stores because bits 3 and 5 are in the set. Membership = non-const objects: `const` tables give `bits=0x2` (only object 1) and
  the loads become free fillers (sink to the block end, 71w) — NOT the target, whose `lwzx` sit before the stores = urgent through the
  store chain (h13 in the model: 2 + `stw stride` 11 <- `stw dst` 9 <- `lwz rfb->pln[0]` 7 <- add/add/`stw src2`). Direct indexing
  `mpvumc_oneref_y[mcflag][vy&1][vx&1]`, `extern` tables, tables never stored in the TU, and a `const MPVUMC_MCFUNC (*tbl_y)[2]` local (cast
  needed; the flow follows the origin object, cf. pass 50's const-parameter rule) all keep idx 3/5 in the set: 48w each. (2) The pick:
  vx is loaded at c22; c24-c32 are full of urgent address/rounding ops; c33 = `lwz ofs[1]` (urgent) + the second IU slot: `rlwinm chx`
  (frees 1, h6 through `and; add cpitch+chx; add; stw src2`) beats `rlwinm yhx` (frees 1, h3: `and`, live-out) on height; c34 = `lwzx fn_y`
  (urgent, dl 29) + `rlwinm yhx` -> vx lives across the load. The target needs yhx at c33 or earlier: yhx height >= 6 (a same-block store
  consumer of yhx — the target has none, its luma src2 is after the calls), frees >= 2 (a second in-block reader with npreds 1; a copy `mr
  @t,yhx` does not count: it puts a WAR pred on the `and`), or chx frees 0 / h < 3 (chx's chain IS in the target: `add r0,r31,r23`). Model
  deletions that flip the order all remove a target instruction (`add cpitch+chx`, `lha ypitch` reload, `lwz pln[0]`, `stw stride/dst`);
  raw positions of yhx after the vx load never do. Moving the fn_y statement below `mc->dst = ..` drops the load to h6 and frees c28 for
  yhx, but then the load follows the stores in the pre-RA output and the post-RA pass cannot hoist it above them (store -> later load
  edge): not the target either. Left 48w.
- **sfh_main `SFH_AnlyElemSmpHz` 6w: the swap block's pre-RA order equals the target's final order, so the fold is not a schedule
  question.** Pre-RA B37 raw = `lwz w; rlwinm x4 (the four terms); mr; rlwimi; mr; rlwimi; mr; rlwimi; stw; li r3,1` (the `|` chain is
  already `mr + rlwimi` on the word, the four term rlwinm are dead), scheduled `lwz | li` c0, `rlwinm acc | rlwinm t1` c2, `mr | t2` c3,
  `rlwimi | t3` c4, `mr` c5, `rlwimi` c6, `mr` c7, `rlwimi` c8, `stw` c9. The RA deletes the three dead terms and coalesces the three
  copies (block dirtied), the post-RA peephole then sees `lwz; li; rlwinm; rlwimi x3; stw` contiguous and folds it to `stwbrx`; the post-RA
  reschedule of the 3-pcode block leaves `lwz; li; stwbrx`. The target's `lwz r6; li r3,1; rlwinm r0; rlwimi x3; stw` is that same order
  unfolded. A pcode between the last rlwimi and the stw cannot come from the pre-RA scheduler (the `li` has only 0-latency WAR/WAW edges
  and takes c0's free slot; an exit-flagged block would keep the raw order, also contiguous), so whatever blocked the vendor's fold sat in
  the RA output after the chain and vanished before the post-RA pass, or the peephole's pattern state differed (a dead term rlwinm still
  present = the RA of the vendor's build deleting dead defs AFTER the peephole is the one ordering that explains it and is compiler-side).
  M4 (`#pragma peephole off` + the asm swap, 10w/38-41w in pass 23) stays unapplied; the C form (6w) stays.

### CRI pass 54 (sfd_mps DecodeOneUnit 13 -> 5w: the IsZero `unit`/`p'` residue is the frontend CSE of `sfd->prm.unit` — the target's unit is a BACKEND temp (the compare's own load, the two later loads merged by load-deletion); adx_dcd5 Ste4AsSte/Mono: no prologue branch exists for a two-def copy web; IN PROGRESS; 2026-09-12)
Harness ~/.cache/cri54/ (`mk.py OUT [--base=B] 'OLD=>NEW'..`, `try.sh NAME [FUNC] [--ra]` / `mtry.sh NAME [--ra]` = variant.sh words + ra.py dump + ghost list,
`w.py RA_DIR [--k N] +ghost:R:phys=P|like=V  move:VID:AFTER  +edge:A:B  name:VID:N --target ..` = chaitin replay with graph edits, `w.sh`/`mw.sh` = the Ste / DecodeOneUnit
target lists, `combos.sh`, `retadj.py DIR` = is ret adjacent to its r3 def ghosts; deleted at the end).
- **DecodeOneUnit `unit` r3 / `p'` r4 (8 of the 13 words) SOLVED in mechanism:** frontend @temps get vids in REVERSE creation order (r51=@707 .. r57=@701, r58=@692:
  the later-created CSE temp @701 sits BELOW the clone parameter @681/@692), so no frontend CSE temp and no own local (`unit = sfd->prm.unit;` declared first or last,
  13w; IsZero loading `sfd->prm.unit` from an `SFD` parameter itself, 13w — the frontend CSEs across the inline boundary too) can ever be coloured before p'. The target's
  unit is a BACKEND temp (vids above every @temp): the compare's load `len >= X + 3` NOT frontend-CSE'd with the argument/store loads, the two later loads merged into it
  by backend-08 load-deletion (one `lwz r3,0x28(r25)`, `addi r0,r3,3`, `mtctr r3`, `cmpwi r3,0`, `stw r3,0(r29)` = the target). Probe `len >= *(Sint32 *)((Uint8 *)sfd +
  0x28) + 3 && sfmps_IsZero(p, sfd->prm.unit)` -> 5w (only cnt r21/r23 left), size equal; the same byte-view on all three reads -> 13w (CSE'd again as identical
  expressions), `(&sfd->prm)->unit` 13w (same AST). Open: the natural spelling that differs in AST from `sfd->prm.unit` for the compare only (a macro over a different
  member path, a `const`/typed view, a parameter of an inlined helper that reads it once) — the byte view is a tagged stopgap.
- **cnt r21 (the other 5 words) = pass 51's exact requirement re-confirmed on the new base (v7 = no pin + `?:` n + the unit view, chaitin IDENTICAL):** ret adjacent to
  BOTH of its own def ghosts (SetErr `mr @t,r3` r89 and CopyPketData r154) -> 14/14 incl. cnt r21; either edge alone or any single `phys=` ghost (r3/4/5/9-12/25/26/30/31)
  leaves 4-5/14. `err = SFLIB_SetErr(..); ret = err;` (register or plain err) range-splits err into two more r3 ghosts (r46/r50) but none adjacent to ret; `ret = err = ..`
  identical graph to the plain form. A straight-line def always kills the old ret at `mr @t,r3`, so the adjacency needs a shape where ret's value is live across its own
  call-result assignment (still open).
- **adx_dcd5 Ste4AsSte / Ste4AsMono (this pass's hypothesis = copies in both arms of an existing branch):** neither target prologue has a branch (`srwi/add/srawi` for
  nfrm/2, no odd/even test); the only branches are inside the frame loop (early return, clamps, inner ctr loop), and any copy of histl/histr placed inside the loop
  overlaps the parameter (live across the back edge for the next copy) -> the `mr` stays (pass 52 part 2). `*histl++ = l1; *histl = l2;` (a second def of the parameter)
  is folded by backend-03 add-propagation BEFORE the last copy propagation, so r35 is single-def again and propagated (115w, no r6 ghost); the in-place parameters that
  DO leave ghosts (src r3, nfrm r4, outl r5, outr r7) are those whose addi survives to the RA. K&R-style parameter lists are rejected by `-lang=c` here. Own-local
  `c1e = c1; c2e = c2;` 126w (size -4). Ghost model on the current a7 (tree minus pin, 115w): phys=9/10 adjacency is only the 13 prologue-live nodes (c1/c2 die at the
  hoisted extsh), so the pass-52 "(#r6+#r8 == 2, #r9+#r10 >= 1) = 18/18" does not reproduce here (6/18 with qtbl moved to vid r50); the pass-52 a7 differed.

### CRI pass 53 (cftyp422_ppc Matching 7 -> 8/8 FLIPPED, 111 OK: Y84C44 29 -> 0w pure C — the CA chain orders the setup, ywidth/yw3 reused for the chroma loop; sfx.h `const src` applied, 15 includers byte-identical; cftfx below; 2026-09-12)
Harness ~/.cache/cri53/ (`v.sh UNIT FUNC NAME SRC [--ra]` = variant.sh words + ra.py dump into ra_NAME; `wi.py PRE2 BLK SPEC` = sched.py on a
hand-written block (`CONST` token = a const-pointer load record); `cwi.py RA_DIR [+a:b,c] [--vid r65=36.5,..] --show names` = chaitin what-if with edited
edges / scan positions; `gen.py OUT ORDER` = the loop-1 setup statements A..J reordered; deleted at the end).
- **Y84C44 `li r0,8` slot 1 (pass 50's open item) = the carry chain, not an unscheduled block.** `srawi` WRITES XER[CA] and `addze` READS it
  (`R0:0:3` operand, SPR class: kind-1 edges write->later reader/writer), so the four `/4`, `/8` signed divisions of the setup are a latency chain in
  STATEMENT order and the pre-RA scheduler cannot pull `hblk = height / 4` (ready at cycle 0 with only a parameter) ahead of the ones written before
  it. Target chain order read off `srawi r4,r10; addze r11; srawi r4,r9; addze r4; srawi r11,r7; addze r24; srawi r6; addze r6` = yskip, cnt, hblk,
  dskip (ours had hblk, dskip first: its `srawi r0,r7,2` filled cycle-0 slot 1, so the `li ofs` sank to slot 10 and the temps took r0 before ofs was
  live). With hblk fourth the only IU ops ready at cycle 0 are `addi d`, `li ofs`, `li i`: ofs (r0) is defined at the block top, interferes with
  every setup temp (target temps r11/r6/r4), and post-RA the `li r0,8` is the first IU candidate after `stwu` (the target's order is a fixed point of
  sched.py --post with const-src load records; 21w after the reorder, entry block identical). Rule for the catalogue: **a group of signed divisions /
  `addze` sign-fixes is scheduled in statement order (CA dependence), so their order in the target listing IS the source order.**
- **Residue (1)+(2) closed together: the target reuses `ywidth`/`yw3` for the chroma loop (`ywidth = src->cbwidth / 4; yw3 = ywidth * 3`, cw/cw3 are
  the same registers r9/r10 as ywidth/yw3), and declares them LAST.** One node each, live across both loops: degree 33/34 instead of 27/26, and with
  the lowest own-local vids no own-local neighbour is removed before their scan-1 turn (degree at the turn 33/34 >= 29 -> level 2, coloured before
  the level-1 loop-transform remainder copy r153, which then takes r11 after r9/r10). chaitin what-if `--vid r65=36.6,r64=36.5` predicted the exact
  colours before the source was written; declared in the other order (ywidth last) = ywidth r10 / yw3 r9. Reuse alone (declared first) 23w: still
  level 1 (10 lower-vid neighbours removed first). So pass 45's "copy needs an own-local vid" was the wrong reading: the copy stays a backend temp,
  ywidth/yw3 move UP a level.
- **Applied (src/lib/cftyp422_ppc.c, pure C, no pins):** `const CFT_YCC420PLN *src`; setup order ywidth, yw2, y0, d, yw3, yskip, cnt, hblk, dskip,
  ofs; `cw`/`cw3` removed in favour of `ywidth`/`yw3` declared after `c`. **src/lib/cri/sfx.h: `const CFT_YCC420PLN *src` in the prototype**; the 15
  includers (sfx_cnv, sfx_lib, sfx_YCC420PLN_to_ARGB8888PLN, sfx_YCC420PLN_to_Y84C44, sfx_zmv, mwsfdcre/frm/lib/ply/set/sfx/sst/svr, mwsfx_ARGB8888PLN,
  mwsfx_Y84C44) deleted and rebuilt under the lock: sha1 identical to before. objects.py `# CRI pass 53` block, `ninja -k 0`, 111 OK.
- **APPLIED (src/lib/sfd_mps.c `sfmps_DecodeOneUnit`, 13 -> 5w, size equal, locked ninja + bytecmp 5w):** the padding test reads the count through the handle's
  creation-parameter view, `len >= ((SFD_CREPRM *)sfd)->unit + 3 && sfmps_IsZero(p, sfd->prm.unit)` (tag `COMPILER-DIFF: M (frontend CSE)`; `(*(SFD_CREPRM *)sfd).unit`
  and the byte view give the same 5w). Negatives (13w, all frontend-CSE'd into one late @temp): nested `if (len >= X + 3) { if (IsZero(p, X)) {..; goto skip_done;} }`,
  an own local `unit = X` (declared first or last), IsZero taking `SFD` and loading X itself, `(&sfd->prm)->unit`; a helper `IsPadding(sfd, p, len, nskip)` and an
  inline `zero` flag loop both materialise a Bool (46w/51w, size +0x10/+4); named `fn`/`obj` locals for the GetCond results 88w (+8: two argument copies survive).
- **cnt r21 (5w left), where the two missing ghosts may sit (model on v7, chaitin IDENTICAL):** two new never-removed nodes with the adjacency of EITHER existing delim
  ghost (r65 = first CheckDelim `mr @t,r3`, r82 = the second), or of the `n` pair (r56, syshd block), or of the GetCond->r4 pair (r84/r86) each give 14/14 (cnt r21,
  bufin/dst r21); two ghosts adjacent to ret alone, or to {ret,wk,data} alone, or with the adjacency of the SetErr/CopyPketData ghosts (r89/r154) give 5/14, and the
  pass-51 pair of edges ret-r89 + ret-r154 stays the only edge-only fix (enumerated: no single edge on ret, no pair among the top 40 candidates besides it). So the
  vendor's +2 coalesced copies sit in the prologue / GetCond / syshd regions (ret, data, wk, sfd, len, nbyte, nskip, mps live; cnt dead) — the same regions pass 52 named.
  Ghost kinds still unfound there without code (a 1-use call result assigned to a named local is propagated: `mr r22,r3` direct, no @temp).
- Tree: sfd_mps 25/26 (5w, M1 pin kept), adx_dcd5 2/4 untouched (25w/143w); objects.py untouched, no flip, no `ninja -k 0`. Harness deleted.

### CRI SWAR kernels pass 12 (mpv_mc 8x8 4p 72 -> 0w APPLIED, mpv_mcy 16x16 4p 119 -> 30w APPLIED: three rotating pixel pairs + the sums reused by the copy-pasted second half = the original's register classes; residue = the first block's cut (needs 10-17 more later-deleted initial instructions before the 9th sum); H2/V2 untouched; nothing flipped; 2026-09-12)
Harness ~/.cache/cri_swar12/ (`mk.py NAME` splices bodies/NAME.c over the 4p function, `t.sh NAME` = variant words + ra.py dump `ra_NAME` +
scheddump `out_NAME` + sched.py check + block sizes, `sc.sh NAME` = pass-11 first-half order score, `s2.sh NAME` = block-2 order score
(`lvmap.py` derives the a9/b9/p8 vregs), `view.py RA OUT TGT LO HI` = every node with label/colour/target/level/degree/class,
`prec.py` = per node the target registers below its colour and which neighbours hold them, `rcsp.py` = randomised CSP over colouring
orders (position statistics + forced before-pairs; `merge=q9:p9,..` unifies webs), `wo.py`/`w.sh` = chaitin what-if on the vid ORDER
by class (`own=` declaration order, `at=` creation order, `be=`, `top=`), `co.py` = greedy colouring with EXPLICIT levels (`l2=`),
`scan1.py` = scan-1 degrees per node under a vid hypothesis, `osearch.py`/`csrch.py` = searches over declaration orders / pixel-pair
classes with the real model, `cls.sh NAME` = class (own/@/be) of every block value. Pass-11 harness deleted at the end.)
- **Forced colouring order of block 1 (rcsp.py, 400 random CSP solutions on the tree's graph, callee-saved handed r21-r31 first):** a6 (r7)
  precedes almost every block-1 node (s2, s3, t1, t2, b3, a0, p0, p1, p2, p5, t0, t3, b4, b5, a4, a5, b6, b7, s4..t6, rot1); a7 (r8) precedes
  a8 < a9, p3 < p4..p8/p0/b7/b8/b9/rot5, rot1, rot5, t0, a0; a2 (r9) precedes a1, b1, s0, t0, b2, b3, t2, a4, b4, t3, a5, b5, s4, t4, b6, s5,
  t5; a3 (r21) precedes p0, p1, p5, t0, b2, a4, b4, t3, a5, b5, b6; p2 precedes a4, b4, t3, a5, b5, s4, t4, b6, s5, t5; the callee-saved chain
  a3 r21 -> {b2, b4, t3, p0, rot5} r22 -> {rot1, p1} r23 -> t0 r24 -> b6 r25 -> a4 r26 -> a5 r27 -> b5 r28 is a strict colouring order (each
  needs the previous one's register taken). The b pixels are all coloured after the temps that consume them (b3 after s2/t2, b4 after s4,
  b1 after s0/t1) = low vids (own locals / @temps); a6, a7 are coloured first of all.
- **Level reading:** with explicit levels (co.py) L2 = {a6 r7, a7 r8, a2 r9, a8 r9, p2 r10, p3 r11, p4 r12} + L1 in descending vid with the
  pixels as own locals declared a0,b0,a1,..,a8,b8 (a6/b6/a7/b7/a9/b9 as @temps above them, b1 as an @temp) reproduces ALL block-1
  volatile colours (33/49; the 16 misses are the callee-saved names, which depend on block 2's handing order p8 r31, p9 r30, p10 r29,
  p13 r28, b14 r27, b13 r26, a16 r25, b16 r24, b12 r23, b11 r22, a13 r21, and p8). The tree's masked form colours 6/49 (all pixels are
  backend temps: reverse creation order). The real model (wo.py, degree scans) gives L2 = {a6, a7, a3, a4, b5, p2, p3, p4} for own-local
  pixels: a3 (38 total, 32-36 at its visit) and a4 (34) always survive scan 1 and a2 (34 total, 28 at visit) / a8 (30, 26) never do, in all
  20160 declaration orders x a/b groupings (osearch.py), with or without the sums declared first; a class search over pixel pairs
  (own / @ / backend, csrch.py, 19683 assignments x 4 @-orders) tops at 26/30 volatile colours. So the target's graph is NOT ours with
  another vid order: a3/a4 must be visited later (backend temps: masked or substituted) while a2/a8 keep >= 29 (own locals/@temps
  with no removed neighbour), or the target's webs differ (a shorter a3 web, extra ghosts on a2/a8).
- **Frontend facts read off the probes (ra.py dumps, cls.sh):** (1) memory operands in the sums (`p0 = s0[0] + s0[1] + s1[0] + s1[1] + 2`,
  probe f1) make the two-use loads CSE @temps numbered a2,b2,..,a8,b8 then a1,b1 LAST (lowest @N = highest vid first: a2), the single-use
  loads backend temps, and p0 is forward-substituted into the d[0] store (p1..p8 are not: their RHS holds the nested CSE load defs) -- the
  raw order then differs (a0/b0 loaded in the store statement) and the block does not split (N1 = 71 initial instructions before d[0]'s
  stw counted from the function start; the split after d[1] needs N1 in [99, 106]: tree 103, q1 103, d1 99 (p8 substituted, 75/73), g1 85,
  h2/k5 (`(Uint8)` at load / `(Uint32)(Uint8)a` at use) 109-111 = split before d[1]). (2) `Uint8` own-local pixels with `(Uint32)` casts
  or without casts (implicit promotion is CSE'd the same way) keep a0..a5, a8, b0..b5, b8 as OWN locals but sink a6, b6, a7, b7 (and the
  pixel-9 web a9/b9) into their CSE temps as @temps (`lbz @t` directly) -- independent of second-half reuse (g1/g6/g7/k1); with no
  reuse at all (fresh names a9..a16, g5) every two-use pixel is sunk (@temps a2..b8, a1/b1 last) and a0/b0/p0/p8 are substituted
  (backend). (3) `(a & 0xFF)` or `(Uint16)a` at every USE on Uint8 own locals (k3/k7, pair form) gives the target's split 79/69 AND the
  target's pre-RA block-1 order (post-RA model 67/67) with own-local pixels (a6/b6/a7/b7/b9 backend) -- 124w; its L2 is {a3, a4, b5, p2,
  p3, p4} (a3 r7, a4 r8, b5 r9). (4) the block-2 sums: in every spelling the second-half p1..p6 are forward-substituted into the d[16]/
  d[17] stores (backend temps, separate q/p nodes, and the d[16] pack then takes rot(p8) as its rlwinm BASE where the target has
  rot(p9) = the `<<14` term like block 1), p7 stays a range-split @temp; the target's block 2 has q_k/p_k in ONE register for k = 9, 10,
  11, 12, 13, 15 (variables) and separate for p14 (r8 = a14's, a temp), so the vendor's second half keeps its sums as variables except
  p14. Pair-form second half (q1) = 119w, block-2 post-RA model 7/56 (10 dataflow-invalid) because of the substituted sums.

### CRI pass 53, part 2 (cftfx StaticV 16 -> 0w pure C APPLIED (4/6, not flipped: Argb420 24w, UserTable 135w); the frontend's web numbering and "last two defs sunk" rules made usable; 2026-09-12)
(Continuation of "CRI pass 53" above; another agent's section landed between. Harness ~/.cache/cri53 deleted at the end.)
- **StaticV 16 -> 0w (APPLIED, pure C, src/lib/cftfx.c):** three frontend rules read off `frontend-02-ast-final-code.txt` of 12 variants:
  (a) a reused single-use variable is split into webs `@N`, numbered PER VARIABLE in order of the variable's FIRST DEF in the function, and within
  a variable in REVERSE statement order (w: rows 4,3,2 = @176,@177,@178; t: rows 4,3,2 = @179..@181) — declaration order does NOT change it (s11);
  (b) the LAST TWO non-constant defs of such a variable are substituted into their uses (the pass-50 "sunk" rows: their values become backend
  temps coloured before the row's d-load); constant defs (`t = 1; y += t`) are folded before the web pass and do not count;
  (c) the level-1 colouring order is descending vid, so of two webs the higher-numbered one is coloured first. Target rows: w r30, hi r11, lo r31,
  d-load r12 in ALL four rows = w's web above t's web in every row, and t's webs never sunk. Form: the low word FIRST into `t` and the high word
  as an expression (`t = lo; d[0] &= hi | K; d[1] &= t | K`: t's own-local/web vid is below the row's backend temps, so hi r11, d-load r12,
  then t r31 — pass 50's "row-4 lo must be a low-vid node"); `Uint32 t = 0;` at the declaration (a dead def deleted later, but it is t's first
  def, so t's webs are numbered before w's -> w's web outranks t's in rows 2-4; row 1 is `w` r35 vs `t` r34 own locals, w declared first);
  and the two pointer round trips `t = (Uint32)y + 4; y = (Uint32 *)t; t = (Uint32)d + 64; d = (Uint32 *)t;` in the inner-loop tail as the two
  defs that get sunk (they fold back to the `addi`s). Negative: no `t` at all (s1, 40w: every row a backend-temp row = ours' old row 4);
  `t = ystep * 4; y -= t` as a sunk def (s4/s16: the substituted `ystep * 4` is CSE'd into a different @temp -> setup colours move, 27-104w);
  defs in the outer-loop tail (s5, 74w, size +4); four distinct t0..t3 (s3, 40w: single-def single-use locals are substituted like sunk webs).
  Catalogue row (MWCC, "two values with swapped registers"): **web rank = first-def order of the variables, then reverse statement order; give a
  reused temporary an early dead first def to rank its webs below another variable's, and two harmless trailing defs to keep the real ones.**
- **Argb420 24w: the two prologue copies are NOT coalescable-but-kept `mr`s of a `?:` — they are real un-coalesced copies with the temps still
  live (`mr r6,r5; add r5,r5,r3` uses the y temp after the copy; the cb temp r4 dies at `mr r9,r4` yet is not coalesced either).** Our RA
  coalesces both (no interference edge at a `mr`, biased colouring), so at RA time the vendor's defs of y/cb were not `mr` pcodes from the load
  temps (or were user copies of a multi-def variable, pass 12's rule). Tried (all 24-25w, size +4 for the a-forms): `half`/`a` from `pln.cb`/
  `pln.y` fields directly (a1/a2/a5), `register` y/cb (a4). Not tried: a `Uint8 *py` that is multi-def (a live second def), an `addi rD,rA,0`
  spelling that survives to the RA. Box over.
- **UserTable 135w, one probe (u1, 137w):** `w1 = ywidth - 4; w2 = w1; w3 = w1; ROW(y); w1 += (Uint32)y; ROW_I(w1); w2 += w1; ROW_I(w2); w3 += w2;
  ROW_I(w3); y = (Uint8 *)(w3 + (ywidth - 4)); y -= ywidth * 4; y += 4;` — the frontend CSEs BOTH `ywidth - 4` into one hoisted @154 and rewrites
  `w2 = w1` / `w3 = w1` to `w2 = @154` / `w3 = @154` (copy propagation of `w1 = @154`), so the `subi` leaves the loop and the copies coalesce.
  The target evaluates `ywidth - 4` TWICE inside the loop (`subi r31,r11,4` at the block top, `subi r26,r11,4` in the fourth step) and keeps
  `mr r7,r31; mr r4,r31`: the vendor's two `ywidth - 4` were not CSE'd by the frontend, i.e. not identical expressions to it (different types /
  casts / a `Uint8 *` pointer difference?), and the copies are of a multi-def variable. Open.
- Tree: src/lib/cftfx.c (StaticV macro + `t = 0` + tail; comments), locked `ninja build/G4BE08/src/lib/cftfx.o`, bytecmp 4/6 (Argb420 24w,
  UserTable 135w); cftfx stays False. cftyp422_ppc flipped in part 1 (111 OK re-checked at the end of the pass).

### CRI pass 55 (sfd_mps DecodeOneUnit 5w unchanged: the +2 requirement is "two more never-removed neighbours on ret, anywhere outside the scan loop"; the frontend's web-naming rule found (first def keeps the variable, later disjoint webs are @temps whose copies are COMPILER copies = coalesced/codeless); the GetCond pair as 2-def locals gives exactly +2 and 14/14 but its first pair costs 8 bytes; nothing applied, nothing flipped; 2026-09-12)
Harness ~/.cache/cri55/ (`mk.py OUT [--base=B] 'OLD=>NEW'..`, `try.sh NAME [--ra]` = variant.sh words + ra.py dump + ghost list + `w.sh` score against the 14
target colours, base v7 = tree minus the M1 pin + `?:` n, chaitin IDENTICAL; deleted at the end).
- **Requirement made exact on v7 with ghostwhatif (pass 54's "5/14 for ret alone" does not reproduce):** two never-removed ghosts adjacent to ret ALONE give
  14/14; so does any pair adjacent to a set containing ret (`names=ret,wk,data[,sfd,len,nbyte,nskip,mps,total,bufin,dst,delim,p]`), and `like=N` pairs for
  every real node N adjacent to ret score 14/14 EXCEPT the scan-loop temps r118-r127 / p / hn / ok (13/14, cnt r23). Exactly 2: one ghost 5/14, three 4/14,
  four 2/14. So the vendor's graph has exactly two more coalesced copies, anywhere ret is live and cnt is dead (prologue, go chain, GetCond, syshd, no_syshd,
  skip-tail, PKET arm). On the if-form base (tree minus pin, nopin) the same target needs THREE ghosts adjacent to {ret,wk,data} (3 like=65 give 6/14).
- **Frontend web rule (v11/v12, read off frontend-01 + the ghost lists):** a variable with two DISJOINT webs keeps its name on the first surviving def
  (a dead initialiser is deleted first, v15) and turns every later web into a range-split `@temp`. Copies into the named web are USER copies (the RA never
  coalesces them: `mr @t,r3; mr obj,@t` stays as two real `mr` whenever the pre-RA scheduler interleaves the next call's `mr r3,sfd` between them — it
  always does for a call result followed by a call); copies into a `@temp` web are COMPILER copies (coalesced, codeless). A single-use load web is propagated
  into its use (no trace); a call-result web is not (the call cannot move) — it survives as `mr @t,r3; mr @temp,@t` and BOTH coalesce (ghost `@t->@temp`,
  `@temp->rArg` when it dies at an argument move). A load into the NAMED web that dies at an argument move is a ghost too (v11: TermOut `buf` first def per
  inlined copy -> r4 ghost; its 2nd/3rd defs propagated): 4 TermOut bodies -> +4 ghosts, 131w, size equal.
- **GetCond pair as 2-def locals `obj = GetCond(OBJ); fn = GetCond(FN); MPS_SetPsMapFn(mps, fn, obj);` twice (v12):** the second (PES) pair is range-split
  (@709/@710) and byte-identical with +2 ghosts (r89->@709, @710->r4) = exactly the requirement, 14/14 including cnt r21 and bufin/dst r21; the first (PSMAP)
  pair is the named web -> `mr r0,r3; mr r21,r0` bounce + `mr r0,r3; mr r4,r0` = +8 bytes (62w). A `void *obj` parameter reused as the local (v14) +12
  bytes; dead `= NULL` initialisers (v15) change nothing; an inlined `GetCondPtr` wrapper with or without a `ret` local (v17/v19) = the `@ret` bounce on all
  four calls, +6 ghosts, 131w. So the vendor's +2 has the SHAPE of one range-split call-result pair; which variable carried a harmless first web is open
  (no `void *`/Sint32 value with a load-to-argument first def exists in the body; TermOut/TermIfInTerm bodies are inlined 4/3 times).
- **Negatives (127w, ghost list unchanged):** `void *obj` parameter + `SFD sfd = obj` (v9: 74w, obj -> r3 ghost with 8 neighbours, sfd r25 right, but sfd
  becomes own local r49 and can never be coloured 7th: 11/14 at best — the target's sfd IS the parameter node r32); `void *obj` on the inlined GetSeeShdr
  (clone copy propagated); GetSeeShdr through a 3-def `ret` local (propagated into @ret); TermIfInTerm returning `t` with the value ignored (dead copy
  deleted); an inlined `sfmps_IsEndcodeSkip` wrapper (compare use propagated); 2-def `err` for DecHd/IsEndcodeSkip (both propagated, the second in the
  frontend, the first by backend-03 in-block copy propagation); `delim = 0; *nbyte = delim; *nskip = delim;` 234w +4. **Codeless neighbour pin on ret is
  impossible:** `asm { mr r11, ret; mr ret, r11 }` after `ret = 0` is constant-folded to `li r11,0` (129w), after the SetErr join backend-02 CSE deletes
  the back copy and the lone `mr r11,ret` adds no node (129w, model 5/14); `asm { mr r11, err; mr ret, r11 }` would emit both `mr` (call-defined err).
- Tree: sfd_mps 25/26 (5w, M1 pin kept); objects.py not touched by this pass, no flip, no `ninja -k 0`. Harness deleted.

### CRI pass 56 (cftfx Argb420 24 -> 0w pure C APPLIED (5/6, not flipped: UserTable 135w); the frontend's forwarding / web / operand-order rules read off ~60 probes; UserTable's in-loop `subi` + copies + increments reproduced structurally (t2 form, 175w, not applied); 2026-09-12)
Harness ~/.cache/cri56/ (`v.sh NAME [FUNC] [--ra|--diff]` = variant.sh words + ra.py dump into ra_NAME; the probe TUs put a
straight-line snippet into `CFT_Ycc420plnToA256V`'s body — a symbol strip_unused keeps — and read it with `variant.sh ... --all --dtk`;
deleted at the end). Tree edits: src/lib/cftfx.c `CFT_Argb420ToArgb8` only; objects.py untouched by this pass (cftfx 5/6, no flip, no `ninja -k 0`).
- **Argb420 24 -> 0w (APPLIED): the whole residue was vid order in the setup, four facts.** (1) The target's `lwz r5 (pln.y); lwz r4 (pln.cb);
  mr r9,r4; mr r6,r5; add r5,r5,r3` = the three reads of `pln.y`/`pln.cb` frontend-CSE'd into two load temps (`@211/@212`, nested-assignment
  CSE) and `y = ..`/`cb = ..` as user copies of them: write `half = (((Uint32)pln.cb - (Uint32)pln.y) >> 1) & ~3; a = pln.y + half;` (fields, not
  the y/cb locals). The RA never coalesces these user copies (no edge, no coalescing: both nodes stay, `mr r6,r6` is only deleted when the
  colours coincide — a1 had the y temp at r6 because r5 was taken by the hblk `srawi`). (2) The y temp must get r5 = the hblk `srawi` temp must
  get r0 = it must be coloured BEFORE the `pln.cbwidth` load: that load was a frontend-hoisted invariant `@208` (created at the preheader = the
  highest setup vid; the frontend substitutes a single-def `cstep = pln.cbwidth / 2 * 2` into its four loop uses and hoists the expression) —
  a two-def `cstep = pln.cbwidth; cstep = cstep / 2 * 2;` makes the LOAD the own local `cstep` (first web = the variable's own vid, the mask
  = `@213`) below every backend temp. Same for `ystep = pln.ywidth; ystep = ystep / 4 * 4` (in bytes, `Uint8 *y/a`, `y += 4` instead of `y++`).
  (3) The two loads are own locals -> coloured by DECLARATION order: `cstep` declared before `ystep` gives cbwidth r7 (first, lowest free) and
  ywidth r10 (`Sint32 cstep; Sint32 ystep;`). (4) The `srawi` temps of wblk/hblk are backend temps coloured by creation order among the setup
  temps: `wblk = width / 4; hblk = height / 4;` placed AFTER `ar`/`gb` and BEFORE the cstep/ystep statements puts hblk's temp above the half chain
  (r0 free at its turn) and keeps it off the step values; and ystep-in-bytes as a STATEMENT (not the hoisted `ystep << 2` @temp) puts its
  `clrrwi r0` before cstep's `clrrwi r30` in the pre-RA input order (class-2 tie -> original order). Steps: a1 24w -> b1 10w (wblk/hblk after the
  steps) -> c1 5w (two-def cstep) -> e1 2w (cstep declared first) -> f5 0w (byte steps, statement order). Negatives: a2/a3 (`half` from the
  fields with `a` from `y`) 25w; c2 `cstep &= ~1` 10w, c3 `cstep -= cstep & 1` 18w; f1/f3 (wblk/hblk after the step statements) 16w; f4 (wblk/hblk
  right after the call) 5w.
- **Frontend rules read off the probes (UserTable; `q*/r*/s*` = straight-line snippets, `t*/u*` = loop forms):**
  (a) **`y = y + K` is FORWARDED into every later use of y in the block whatever the use count** (q5: two uses, both rewritten), and the codegen
  reassociates `p + (y + K)` into `add p, y, p; addi p, p, K` — the y-FIRST operand order comes only from this reassociation (q1/q2/q6/q17/q27-q32:
  `(Uint32)(y + 4)`, `(Sint32)`, integer y, `(y += 4)` inside, `&y[4]`, `(Uint32 *)y + 1` inside a cast — all reassociate). Forwarding is blocked
  by post-increments (`*y++` x4 are merged into ONE `addi y,y,4` that stays a real def, q11-q13/t2) and by a pointer-cast round trip as a full
  assignment `y = (Uint8 *)((Uint32 *)y + 1)` (q24, but that is a new web/temp, t7).
  (b) **Codegen operand order = AST order:** `p += e` (EADDASS) -> `add p, p, e` (q3/q10/q12/r2-r13: no cast, difference or extra use flips
  it); `p = a + b` (EASS) -> `add p, a, b` (q4/q13/q19/r13). **A full assignment EASS starts a NEW web even when its RHS reads the variable**
  (`p2 = y + (Uint32)p2` -> `@160`, frontend-01); EADDASS/EPOSTINC continue the web. So "in-place add into the accumulating vreg with the added
  value FIRST" (`add r31,r6,r31`) is not writable as `+=` and a full assignment splits the web.
  (c) **Backend loop-code-motion (pass 04) hoists a single-def invariant vreg; a two-def vreg stays.** `p2 = (Uint8 *)(ywidth - 4); .. p2 +=
  (Uint32)y` keeps `subi` in the loop and its user copies `p3 = p2; p4 = p2` survive (the source is redefined before their uses: `mr r8,r7; mr
  r9,r7` = the target's `mr r7,r31; mr r4,r31`); with a web split (b) the first web is single-def -> backend copy propagation removes the copies
  -> the `subi` is hoisted (t10/t12/t13/t19/t22/t23/t25, all "collapsed"). The frontend does not hoist a variable's own invariant def (`p2 =
  (Uint8 *)(ywidth - 4)` stays; only expression @temps are hoisted), and it does not CSE `ywidth - 4` with `(Uint32)ywidth - 4` (u2: the typed
  copy of the tail's second `subi` stays separate; with identical types both become one hoisted @temp, u1/u4).
  (d) **A 4-iteration constant `for` loop is fully unrolled by `-O4` into the same object as the hand-unrolled rows** (k1/k2: UserTable
  byte-identical to the tree's 135w) — but its presence changed the OTHER functions of the TU (.text 0xb6c -> 0xd78: CFT_MakeArgb8888ColAdjTbl /
  MakeYcc422 grew, i.e. the unroller's budget is TU-wide state); `k += 2` (k3) is not unrolled.
  (e) The or->rlwimi peephole's `mr v, B` into an OWN local `v` DOES coalesce when no edge exists (t2/m1-m5/n3/n6/n7: `slwi` straight into v's
  register) — pass 5's "never" needs the edge (v live at B's def, or B live after the copy). Byte locals `b0..b3` (m3) add a `clrlslwi` fuse.
- **UserTable 135w, the target's shape now readable (per inner iteration):** `subi w4 = ywidth-4` + `mr w4b,w4; mr w4c,w4` at the top; row 1 on
  `y` with `addi y,y,4` then `add w4 = y + w4` (dest = second operand, in place); row 2 on w4, `addi w4,4`, `add w4b = w4 + w4b`; row 3 on w4b,
  `addi`, `add w4c = w4b + w4c`; row 4 on w4c, a SECOND `subi t2 = ywidth-4`, `addi w4c,4`, `add t2 = w4c + t2`, `subf y' = t2 - yw4`, `addi y = y'+4`;
  row 1's two packs go through `mr r25,rX` copies (an own local), rows 2-4 pack in place (@webs/sunk). **Best structural form t2 (175w, not
  applied — the tree's 135w is closer by count):** `Uint8 *p2,*p3,*p4,*p5; Uint32 v0,v1;` macro `v0 = ((Uint32)tbl[*(y)++] << 24) | ((Uint32)tbl[*(y)++]
  << 8); v1 = ..; (dst)[0] &= v0 | K; (dst)[1] &= v1 | K` and body `p2 = (Uint8 *)(ywidth - 4); p3 = p2; p4 = p2; ROW(d, y); p2 += (Uint32)y; ROW(d+2,
  p2); p3 += (Uint32)p2; ROW(d+4, p3); p4 += (Uint32)p3; ROW(d+6, p4); p5 = (Uint8 *)((Uint32)ywidth - 4); p5 += (Uint32)p4; y = p5 - ywidth * 4; y +=
  4;` = in-loop `subi` + both `mr` copies + all four `addi p,p,4` as real defs before their adds + the second `subi` + `stmw r25`/frame 0x30.
  Left in t2: (A) the four adds read `add X, X, e` (ours) vs `add X, e, X` (target) — per (b) the target's AST had the added value first AND one
  web: not reachable with `+=`, and `p2 = y + (Uint32)p2` splits (t19), `p2 = (Uint8 *)((Uint32)y + (Uint32)p2)` splits (t12), integer p2 forms
  are forward-substituted into `y + @hoisted` (t13/t17/t23), a helper parameter is propagated (t25); (B) row 1's `slwi B; mr v,B; rlwimi v` copies
  (needs the edge of (e): not from v0/v1 own locals, one `v`, `v = v1` chaining, live-out v, `|=` forms, `t`/`a0` operand locals — m1-m5, n3, n6,
  n7); (C) colours: y/d are level 1 in ours (r29/r30, after the pack temps) and volatile r6/r12 in the target, w4 r7 vs r31, yskip r31 vs r30,
  i/wblk/hblk/ywidth shifted by one. Next: (A) first — find the AST that yields EADD(e, X) assigned to X's own web (maybe the vendor's X is a
  parameter of a NON-inlined helper? no: one function in the target), then read (C) with chaitin.py on that form; (B) after.
- Negatives with the tree's macro (all >= 135w unless noted): u1 (pointer copies, identical `ywidth - 4` twice) 160w, u2/u5/u6/u8 (typed tail
  subi as p2/p3/new p5) 184w, u7 163w, u9/u10 (v0/v1 + increment before the stores: no blocker — a store blocks only defs that LOAD) 185w, t5
  `register y` 185w, t6 integer y 185w, t7 cast-round-trip increments 173w, t8 `y + 2 + 2` / t9 `y += 0` 185w, t20/t21 `(y += 4)` inside the add
  189/184w, t14/t15 188-190w.
- **Cut rule made exact (B3-relative initial-code counts, `awk` over `backend-00-initial-code.txt` between `B3:` and `B4:`):** the codegen
  ends the block at the first STATEMENT END where the block's count is >= 101 (tree: d[1] ends at 105, d[0] at 97; w3i: 101 after
  `b2 = s1[11]` with `a2 = s0[11]` at 100 not taken; c1000: 101 = the 9th sum's last add = the target's cut). w3i has d[1] at 83 and the
  9th sum's end at 91; the vendor's d[1] ended in [93, 98] (then `a0 = s0[9]`, `b0 = s1[9]`, the sum's 6-7 instructions reach 101).
  **c1000 = Uint8 pairs with the cast on the FIRST operand only (`p0 = (Uint32)a0 + a1 + b0 + b1 + 2`)** puts the cut exactly there
  (the `(Uint32)a_k` and the int promotion of the same pixel in the previous sum are two different casts = 3 later-deleted instructions
  per sum instead of 2; d[1] at 92) with the target's block sizes 79/69, but Uint8 pairs cost the levels: a8 is removed at 27/30 (its
  three own-local neighbours p5, p6, p7 go first) and p8 is L1, so L2 shrinks to 10 nodes and the five loop variables (24-27 at scan 2)
  fall into L2 -> 136w; with `p0 = p1 = .. = 0` before the loop (c1000_j) the loop variables are L3 again and block 1 is 24/30 volatile
  (117w) but a8 stays L1. With Uint32 pairs every partial-cast pattern is a no-op (no count change), masks on one pair per sum are folded
  by the frontend (u1-u3: 105w, count unchanged), `(Uint32)(Uint8)a` likewise. So the open item is one construct that (a) adds 10..15
  later-deleted initial instructions before d[1]'s end without changing the Uint32 webs (per-sum +2 or per-store +5..8), or (b) keeps a8
  at >= 29 with Uint8 pairs and the right cut (the target's a8 has 2 neighbours more than ours at its visit, or p5..p7 are not removed
  before it). Harness kept: `ev.sh NAME` prints words, block sizes, block-1 colour score and the L2 set (needs ~/.cache/cri_swar11/colmap.py
  + t_4p16.txt, rebuilt there after the pass-11 harness was deleted; the pass-11 order-scoring tools whatif/post/cmp/sym are gone).

### CRI SWAR kernels pass 13 (in progress; 16x16 4p cut probes negative with Uint32 pairs; 8x8 H2 target = TWO ROWS per iteration (ctr 4) + in-place averaging; 2026-09-12)
Harness ~/.cache/cri_swar13/ (`gen.py NAME key=val` writes bodies/NAME.c of the 16x16 4p three-pair form: `ptype=Uint8[,U8,U32 per pair]
sum='(Uint32){a} + ..' pack=or|intr|raw store= dcbt= head= order=pix|loop`; `p.sh NAME [FUNC] [UNIT]` = variant words + ra.py dump + `cnt.py`
(B3 initial count at every statement end, marks d0/d1/s9/cut by statement text) [+ `SC=1` scheddump + block-1 colour score]; `sum.sh NAME` one
line; `h8gen.py` = 8x8 H2 two-row loop from the tree's case bodies; deleted at the end).
- **16x16 4p cut, Uint32 pairs (tree 30w, d[0] 75 / d[1] 83 / 9th sum 91 / cut 101 after `b2 = s1[11]`): every probe leaves the count unchanged:**
  identity ops on the operands (`+ 0`, `| 0`, `* 1`, `& 0xFFFFFFFF`, `<< 0`, `>> 0`, `^ 0`, `- 0`: all folded by the frontend), `(unsigned int)`/
  `(int)`/`(Uint32)(unsigned int)` casts (the frontend keeps ETYPCON nodes but a same-size single-use conversion emits nothing; only a CSE'd
  @temp cast is a `mr`), `unsigned int` pixel or sum locals, the dcbt argument spelled `(int)stride`/`(Sint32)stride` (the `@12 = (int)stride`
  copy is HOISTED to the preheader by the frontend: it never counts in the body), intrinsic packs (`__rlwimi` chain = rlwinm + 3 x (mr + rlwimi)
  = 8, the same 8 as the macro's 4 rlwinm + 3 or + stw; 117w), inlined helpers `sum4(a,b,c,d)` / `plus2(x)` / `pack4(p0..p3)` / a dcbt wrapper
  (every parameter is substituted by the frontend: zero copies, pass 12's "4 argument copies" does not reproduce; 136w for the sum helpers
  because the cast @temps change). 
- **Uint8 pairs:** all four `(Uint32)` casts (x3) = +2 only (the single-use pixel-0 zero-extensions), 106w, L2 = {a6 a7 a8 a2 p2 p3 p4} (target set,
  no p8), block-1 volatile 28/30, and NO backend CSE pass in the pipeline; ANY non-CSE'd second cast of a pixel (`(Uint32)a` vs implicit int, or vs
  `(unsigned int)`/`(Sint32)`/`(int)`, on a or on b pixels: x0/y1-y5/w4) = d[1] 92 / 9th sum 101 = the exact cut, but the pipeline gains the
  `common-subexpression-elimination` pass (duplicate `rlwinm 0,24,31` of one load) and a8/p8 drop to L1 -> 136w in every spelling; `(Uint16)b`
  as the second cast = 2 rlwinm (uchar->ushort->int) + the CSE pass, d[1] 100, cut after `a0 = s0[9]` (one statement short), 116w with the target
  L2; `(Sint16)` casts = extsh kept (147-190w); `Sint32` sums 136w. Pipeline fact: the backend runs `constant-propagation` + `load-deletion` only
  when the codegen emitted zero-extension masks, and `common-subexpression-elimination` only when it emitted duplicate expressions.
- **8x8 4p is byte-identical with `Uint8` pairs + all casts as well as with `Uint32` pairs (m8u8/m8u32)** — it does not discriminate the
  vendor's pixel type. The 8x8 H2 target (mpv_mc, 436w, ours 200 bytes smaller): `li r0,4; mtctr` — **two rows per iteration** (d[0..3],
  `addi d,16`), no frame, no callee-saved (row temps r0,r4,r5,r8,r9,r10; s r7, d r6, stride r3, masks r11/r12 hoisted `lis/subi`,`lis/addi`), per
  row: dcbt, lwz w0, lwz w1, lbz w2, `slwi a0 = w0<<8; rlwimi a0, w1, 8,24,31`, `rlwimi w2, w1, 8,0,23` (a1 fused INTO the lbz register), xor x0,
  `add s,s,stride` (mid-row), `and w0,w0,a0` (in place), xor x1, and a0 = x0&m1, `and x0,x0,m2`, `and w1,w1,a1`, and a1 = x1&m1, `and x1,x1,m2`,
  srwi a0, add w0 += x0, srwi a1, add w0 += a0, add w1 += x1, stw d[0], add w1 += a1, stw d[1] = the AVG2 written IN PLACE (`w0 &= a0; a0 = x0 &
  m1; x0 &= m2; a0 >>= 1; w0 += x0; w0 += a0; d[0] = w0;`). Case 1: lhz + `rlwimi w0,w1,0,0,15; rotlwi w0,16`; case 3: `lwbrx` + rlwimi (the
  tree's `__lwbrx` intrinsic form). Probe h8c (case 0 only, two rows, in place, tree's a0/a1 or-forms): the row is the target's 26 opcodes; ours
  hoists `and (x0 & m1)` above `xor x1`/`and w0,a0` (peak 12 live -> m2 in r31 + frame) where the target issues `and w0,a0; xor x1; and (x0&m1)`.
  **The 16x16 H2 target's `srwi r7,w1,24; mr r22,r7; rlwimi r22,w0,8,0,23` is the in-place spelling `a0 = w0 << 8; a0 |= w1 >> 24;` (h8b
  reproduces `srwi; mr; rlwimi ..8,0,23` exactly); `a1 |= w1 << 8` gives `slwi; or` (no fusion) — the 8x8 target's `rlwimi w2,w1,8,0,23` needs
  the or-expression `a1 = (w1 << 8) | a1`.**

### CRI pass 57 (cftfx UserTable 135 -> 109w pure C APPLIED (5/6, not flipped): the `add X, Y, X` operand order, the target's level-2 colouring, the in-loop second `subi` and the `subf temp; addi y` tail all found; the block-split rule (> 100 pcodes) read; left: row-1 `mr r25` copies, the split point, the level-1 cascade; 2026-09-12)
Harness ~/.cache/cri57/ (deleted): `gen.py`/`mk.sh`/`mk2.sh` body generators on top of the t2 form, `dis.sh NAME` = variant.sh words + a
cleaned dtk listing `ours_NAME.s`, `v.sh NAME --ra` = ra.py dump, `pc.py RADIR` = the loop body's PCode with `vreg=colour` annotations (the
reading tool of this pass), scheddump/sched.py `--block N --verbose` for the heights. Tree edit: src/lib/cftfx.c `CFT_A256_ROW` + the function
only; objects.py untouched (cftfx 5/6, no `ninja -k 0`).
- **(A) solved: `X += (Uint32)(Y + 4) - 4` is the one-web spelling of the target's `addi Y,Y,4; add X, Y, X`.** Probes (t2 body): `X = e + X`
  (a1: `(Uint8 *)((Uint32)y + (Uint32)p2)`) -> `add X', e, X` and `X = X + e` (a3) -> `add X', X, e`: the codegen keeps AST order but BOTH are
  full assignments = a new web X', the first web (subi + the two copies) is single-def -> copies propagated, subi hoisted (176w). `X = X + e`
  WITHOUT a cast round trip (i9 `p2 = p2 + (Uint32)y - 0`) is turned into EADDASS by the frontend (same as `+=`: `add X, X, e`, one web).
  `+=` with `e * 1`, `0 + e`, `e >> 0`, `e ^ 0`, `(Sint32)e`, `-= -e`, `(y - 0)`, `e + 0 * ywidth` (i1-i8): all folded, `add X, X, e`. The
  codegen of EADDASS(X, EADD(Y, K)) is `add X, Y, X; addi X, X, K` (pass 56 (a), the AST keeps `p2 += (Uint32)(y + 4)` unreassociated, j3's
  frontend-01) — so `p2 += (Uint32)(y + 4) - 4` (c1; also `(y - 4) + 4`, `&y[4] - 4`) gives `add X, Y, X` with the +-4 cancelled by the backend
  and the row's own real `addi Y,Y,4` before it, one web, subi and copies kept (`add r31, r12, r31` = the target's `add r31, r6, r31`).
  The frontend does NOT fold `(Uint32)(y + 4) - 4` (the +4 is inside the pointer cast); it folds every integer-level +-0.
- **The second `subi` (row 4's fresh `ywidth - 4`) stays in the loop as `p5 = (Uint8 *)(ywidth - 4)` (e1)** — identical to p2's def and NOT
  CSE'd with it here (pass 56 u1/u4 said "one hoisted @temp": that was a different body); `(Uint8 *)((Uint32)ywidth - 4)`, `(Uint8 *)ywidth
  - 4`, `(Uint8 *)0 + (ywidth - 4)`, `(Uint8 *)(ywidth + 4) - 8` are all frontend-hoisted @temps (`@156`, an extra r0 that shifts every
  level-2 colour by one: d -> r31). Rule: EADD(var, const) directly under the pointer cast is not hoisted; any cast/expression inside the
  EADD is. e1 alone: 178 -> 113w.
- **(C) solved by declaration order alone; chaitin.py `--check` IDENTICAL on every dump.** The target's level 2 (12 nodes, one level, vid
  order) is yw4 r0, dskip r3 (the two hoisted @temps), then p4 r4, y r6, p3 r7, i r8, wblk r9, hblk r10, ywidth r11, d r12, p2 r31, yskip r30:
  own locals in reverse declaration order -> declare `Uint8 *p4; Uint8 *y = src->y; Uint8 *p3; Sint32 i, j, wblk, hblk, ywidth; Uint32 *d;
  Uint8 *p2; Sint32 yskip, dskip; Uint8 *p5; Uint32 v0, v1;` (d1/e1). j (ctr) and dskip (single-def, substituted into `d += dskip`) are not
  nodes. p5, v0, v1 are level 1 (coloured after the temps: p5 r26 = the target with the m1 rows below).
- **`y = p5 - ywidth * 4 + 4` = the target's `subf r4, r0, r26; addi r6, r4, 4`** (f1, 111w): the subtraction is an expression temp (r4) and
  `y` starts at the addi; `y = p5 - ywidth * 4; y += 4;` puts the subf into y's own web (`subf y; addi y, y, 4`).
- **Row spelling: `tbl[((y) += 4)[-1]]` for the last byte (m1, 109w)** = one real `addi y,y,4` after the loads (1 pcode) with indexed loads
  (`y[k]`: lbz, lbzx, shift = 3 pcodes per byte, no `clrlwi`); `*(y)++` x4 = 5 pcodes per byte (lbz, addi, clrlwi, lbzx, shift; 32 per row,
  f1 111w); a statement `y += 4` anywhere in the row (before or after the stores, `(y += 4)` inside the add, `y = (Uint8 *)((Uint32)y + 4)`
  round trips; h1/h2/j1-j4/k2-k4) is forwarded into the next add and its +4 sunk to before the NEXT row's add (`add p2, y, p2` early, loads at
  +4..+7, `addi p2,p2,4` right before `add p3, p2, p3`) — a store between the def and the use does NOT block this forwarding (pass 5's blocker
  list is for single-use substitution). Integer step variables (n1) are substituted and hoisted (`add r6, r0, r6`).
- **Block split rule (read off g1/f1/m1): the backend starts a new block after the first STATEMENT that brings the block's initial-code
  pcode count above 100** (B4 = 101/103/104 pcodes; macro statements on one source line split between them; the frontend hoisting/forwarding
  runs before). The inner body is therefore two blocks (B4 rows 1-3(+), B5 the rest with LOOPWEIGHT=8 vs 64) and the scheduler cannot move
  across the split. The TARGET's split is right after `p4 += ...` (its `add r4, r7, r4` is scheduled inside row 3's packs, its `subi r26`
  at the top of row 4 and `add r26, r4, r26` before row 4's last store): with S setup pcodes (subi + copies), R per row, A per add:
  S + 3R + 2A <= 100 < S + 3R + 3A. Ours: f1 (S3, R32, A2 = the c1 add is `add` + a self-`mr`) = 103 -> split before `p4 +=`; m1 (S3, R25,
  A2) = 82 -> split after row 4's first store. Candidates for the original: (R31, A2, S3), (R30, A3, S3), (R32, A1, S2), (R30, A2, S5) —
  a row spelling with 30-31 pcodes or a 1-pcode `add X, Y, X` (the EASS `X = Y + X` is 1 pcode but splits the web) was not found.
- **(B) read, not solved:** in the target both row-1 packs go `slwi B; mr r25, B; rlwimi r25, A` with v0/v1 = r25 (a NEW callee-saved: the
  own locals are coloured last and every one of r26-r31 is a coloured neighbour); in ours v0 lands on B0's colour (the user copy is never
  coalesced, the `mr` is only deleted when the colours coincide, pass 56 (1)) and v1 keeps its `mr`. The level-1 colours diverge from the
  first row-1 temps: tbl[y0] r28 (target r29), B0 r27 (r28), y3 r29 (r27). Cause found with sched.py: y[2] and y[3] loads tie at h=54 (both
  packs' chains are equalised by the alias chain through the stores) and the earlier input order wins -> ours loads y2 first, the target y3
  first (`lbz r27, 3(r6); ...; lbz r26, 2(r6)`), which puts y3 in r27 below the oris/and temps; writing the second pack with the byte-3
  operand first (o1/o2) fuses the other operand (`rlwimi ..., 8, 16, 23`, pass 5's earlier-defined rule) — the target's y3-first order is a
  HEIGHT difference in its DAG (a different split point or row spelling changes the chain lengths), not the source order. So (B) and the
  level-1 cascade are downstream of the split point / row spelling above; fix that first, then re-read with pc.py.
- Setup residue (post-split): `subf r7, r10, r11` (ywidth - width) is scheduled before `lwz r12, 0(r4)` (d) and `slwi r0, r11, 2` (yw4)
  before `add r30` (yskip) in the target, after them in ours — the statement order of the setup (yskip's expression before d's load?) once
  the body matches.

### CRI pass 58 (cftfx UserTable 109 -> 73w pure C APPLIED (5/6, not flipped): the block split put after `p4 += ...` with 6 deleted mask pcodes per row, `const Uint8 *tbl` = the table loads' own alias class -> the target's two blocks 77/28, frame `stmw r25`, both row-1 `mr r25` copies and size 0x22c exact; left: the level-1 temp colours (rows 3-4 load d[k] before the pack's rlwimi in the target, ours one slot later) and the setup `slwi r0`/`add r30` order; 2026-09-12)
Harness ~/.cache/cri58/ (KEPT, small: `mk.py NAME [--base=B] 'OLD=>NEW'..`, `t.sh NAME [--ra]` = variant words + ra.py dump + `cnt.py` initial
counts per source line for B4/B5 + final block sizes, `dis.sh NAME` = cleaned dtk listing `ours_NAME.s` + diff against `target.s`, `pc.py RADIR [B]` =
pre-RA PCode with `vreg=colour`, `chk.sh [schedwhatif edits]` = the target's B5 interference constraints as 0/1 under a DAG edit, `postwhatif.py` /
`score.sh` = post-RA replay of a pre-RA order with the target's colours against the target's B5, `wo.py` = chaitin what-if on vid order / edges;
`cur.c` = the tree, `ra_u1`/`out_cur` = its dumps. Delete it when the cascade below is closed.)
- **Split (APPLIED): the initial count is 25 pcodes per row (3 per byte + addi + 2 or + 2 x (oris, ori, lwz, and, stw)), 2 per `p += (Uint32)(q + 4)
  - 4` (`add`; `mr X,X` from the -4), 3 setup -> row 3 ends at 82, `p4 +=` at 84, split inside row 4 (B4 = 101/104).** Later-deleted pcodes that
  leave the final code untouched: `& 0xFF` on a byte INDEX (`tbl[y[k] & 0xFF]`: +1 `rlwinm 0,24,31`, deleted by load-deletion, 107w = colours only)
  and `& 0xFF` on the `<< 24` VALUE (`((Uint32)tbl[..] & 0xFF) << 24`: +1, the mask folds into the `rlwinm 24,0,7` that the or->rlwimi peephole
  bypasses; the leftover is a dead def deleted at RA, degree 0/0). A value mask on the `<< 8` operand FUSES (`clrlslwi 24,8`, 8 of them: final code
  changes, m2/m3) -- never mask the base operand. 4 index + 2 value masks = 31 per row: 3 + 3x31 + 2x2 = 100 <= 100 < 102 -> B4 ends after `p4 +=`
  (init 102), final B4/B5 = 76/28 (m4, 98w). Uniform per row, so the macro carries the masks. The peephole leftovers (`rlwinm 24,0,7`, h=1) exist in
  EVERY spelling (7 in B4 without masks) and are RA-deleted; they matter only as "frees" successors in the scheduler's tie-break (row 3: `lbzx tbl[y2]`
  beats `lwz d0` at c42 because its dead shift has npreds 1).
- **`const Uint8 *tbl` (APPLIED, 98 -> 83w, size exact, B4 77 = the target's): a `const` POINTER PARAMETER gives its loads an object alias record
  (`alias=t0@..`), disjoint from the stores' bitset class `t2`; a `const Uint8 *` LOCAL (y, p2..p4, or a copy `tb = tbl`) stays `t2` = the stores'
  class (t4/t2 probes).** Without the load->later-store edges the tbl loads' heights are their chain heights, so the two packs' loads stop tying
  (pass 57 (B): the alias chain through the stores equalised them): byte 3 (the `<< 8` base, slwi + mr + rlwimi) is loaded before byte 2 by HEIGHT,
  the tail `addi p4; add p3'; subf; addi y` falls after the last store (and1 = r4 as in the target), `add` after `oris v1|K` (p4 live -> v1|K r6).
  `const CFT_YCC420PLN *src, const CFT_ARGBDST *dst` (as StaticV, the vendor's) -> 73w with both row-1 `mr r25` copies and `stmw r25` (frame 0x30).
  Wrapper `CFT_Ycc420plnToA256V` keeps `Uint8 *tbl`.
- **Fourth step as p3's second web (APPLIED, same 73w):** `p3 = p2; p4 = p2` AFTER row 1, and `p3 = (Uint8 *)(ywidth - 4); p3 += (Uint32)(p4 + 4) - 4;
  y = p3 - ywidth * 4 + 4;` -- the range-split web `@164` is numbered above v0/v1's row webs (`@158..@163`: web numbering = first-def order of the
  VARIABLES, p3's first def now follows v0/v1's), so it is coloured before them and takes r26 (= the target's `subi r26`, live across B5); the
  target's B5 packs are r27 because r26 is taken and r6/r7 are held by y3/d0 (below). With `p5` (pass 57) the own local is coloured last and gets r26
  too, but nothing above the packs holds r26.
- **Setup order:** `subf r7,r10,r11` before `lwz r12` now matches; `slwi r0,r11,2` (hoisted `@155`) before `add r30` (yskip) in the target, after it
  in ours (B2 is RA-dirtied and post-RA rescheduled; s1-s3 = yskip after dskip / an own-local `yw4` / operand order: 73/77/75w, no).
- **Level-1 cascade, read exactly (B5 = row 4, coloured first; then rows 3, 2, 1 in descending vid) with `pc.py` + the target's registers:**
  colouring order per row: and1, v1|K, d1, and0, v0|K, d0, tbl[y3], y3, tbl[y2], y2, tbl[y1], y1, tbl[y0], y0, then the @temp webs, then own locals.
  Target B5: subf r4, and1 r4, v1|K r6, d1 r7, and0 r6, v0|K r6, d0 r7, tbl[y3] r31, **y3 r6**, tbl[y2] r29, y2 r29, tbl[y1] r6, y1 r6, **tbl[y0]
  r31**, y0 r7, packs r27, p3' r26; ours: y3 r31, tbl[y0] r7, packs r6/r7 (the rest equal). Necessary pre-RA linear-order facts of the target
  (chaitin model, `--check` IDENTICAL on ours): (1a) `lbzx tbl[y3]` BEFORE `oris v0|K` (y3 and v0|K share r6); (2) `lwz d0` BEFORE `rlwimi v0`
  (tbl[y0] = r31 needs r7 = d0 taken); (4) `lbz y3` AFTER `slwi B0` (tbl[y1] = r6 = y3); (11) `slwi B1` after `oris v0|K`; (3a) subf after the
  last stw; (3b) `oris v1|K < add p3' < and1`. Ours (sched.py IDENTICAL): (4)(11)(3a)(3b) hold; (1a) and (2) fail by ONE position each: c6 =
  {rlwimi (h11), lwz d0 (h10)}, c7 = {oris (h10), lbzx tbl[y3] (h9)} -- the IU op is picked first by height. Row 3 (B4) shows the same two
  off-by-one facts (target tbl[y3] r31 = d0 overlaps it -> `lwz d0` before `slwi B1`; tbl[y1] r28 = y3 overlaps it -> `lbz y3` before `slwi B0`),
  and row 1's colours follow from rows 2-3 handing out r27 (target row 2 tbl[y1]/y2/tbl[y2] r27). The post-RA replay (`score.sh "mv=11:10"`) of
  ours' pre-RA order with the target's colours and `lbzx tbl[y3]` moved before the oris reproduces the target's B5 except the tail `addi y`/`addi d`
  tie (input order: in ours the dead shift leftover blocks `addi y` at c16 by the IU same-GPR rule).
- **What does NOT produce (1a)+(2) (schedwhatif on B5, `chk.sh`):** y3's chain before y2's in input order (only a tie-break; the or's FIRST operand
  is the one inserted, so `tbl[y3] << 8` first gives `rlwimi 8,16,23` -- o1), `lwz d0` first in input order, the y loads in tbl's alias class,
  removing the dead shift leftovers, no web `mr` (sunk pack), an extra copy in the pack-0 web chain (gives (1a), loses (2): at c6 the LSU takes
  `lbzx tbl[y3]` (frees 1) over `lwz d0` (frees 0)), an extra copy after `lwz d0`. Arithmetic of the requirement: the LSU issues one load per
  cycle; after tbl[y0] (c3) the target needs y3 (c4), d0 (c5), tbl[y3] (c6), y2 (c7) while ours has y3, y2, d0, tbl[y3] -- i.e. `lwz d0` must
  beat `lbz y2` at c5 (equal height 10; y2 frees its lbzx, d0 frees nothing) AND `lbzx tbl[y3]` (h9) must beat `lbz y2` (h10) at c6, or y2 must
  not be ready before c7. One deleted instruction in the d0 chain with the lwz earlier in input order gives only the first. Not found: a spelling
  that delays the y2 load (its `lbz 2(p4)` uses the pre-increment base in the target, so it is not the unfolded `-2(p4')` form) or lengthens the
  y3 chain by 2 and the d0 chain by 1 with nothing in the final code.
- Tree: src/lib/cftfx.c (prototype + function + `CFT_A256_ROW`), locked `ninja build/G4BE08/src/lib/cftfx.o`, bytecmp 5/6, UserTable 73w size
  0x22c/0x22c, opcode multiset identical (all 73 words are register names / post-RA order). objects.py untouched, no flip, no `ninja -k 0`.
- **Late probes (not applied): the or->rlwimi peephole inserts the operand whose shift is DEFINED EARLIER in the pcode order, not the or's
  first operand** (q1: `t = tbl[y3] << 8; v1 = (tbl[y2] << 24) | t;` keeps the or's operand order but defines the `<< 8` first -> `rlwimi 8,16,23`,
  4 of 8 packs wrong, 77w; judge shapes with `shape.sh NAME` = register-blind mnemonic+immediate multiset, the plain opcode multiset misses it).
  So "y3's chain first in input order" is impossible with the correct fusion unless both bytes are loaded before either shift; byte-value locals
  `b0/b1` reused per row (r1) do that but lose the row-1 own-local copies (frame 0x20, 101w).

### CRI pass 60 (sfd_cre Matching 5 -> 6/6 FLIPPED, 111 OK: sfcre_AnalyMpv 15 -> 0w pure C — a mask-then-shift test `((b7 & 0xF0) >> 4) == 0` leaves a scheduler-visible dead def; adx_tsvr 2w / sfh_main 6w / sfd_mps 5w / mpv_umc 48w / mwsfdcre 4w+115w read, not addressed by the pass-58 levers (the target's own load positions prove OneReadMb's rfb/mv were not const); 2026-09-12)
Harness ~/.cache/cri60/ (`mk.py SRC OUT [MARK=func] 'OLD=>NEW'..`, `t.sh UNIT FUNC VARIANT.c` = one word line via variant.sh, `sd_cre/` scheddump, `ra_t`/`ra_d` ra.py dumps; deleted at the end).
- **sfcre_AnalyMpv 15 -> 0w (APPLIED, FLIPPED):** pass 49's requirement ("the DAG must change: `addi ofs+1` after the `cmpi`, `lbz b5` after the addi") is met by
  spelling the picture-rate test as `((b7 & 0xF0) >> 4) == 0` instead of `((b7 >> 4) & 0xF) == 0`. Read off the dumps (ra_d): the codegen emits `rlwinm r50 =
  b7 & 0xF0; srawi. r51 = r50 >> 4` (a signed shift of a masked int is RECORD-FORM at codegen: the `== 0` compare is folded into it, no `cmpi` pcode), backend-01
  peephole-forward folds the mask into the shift (`rlwinm. r51, b7, 28,28,31`) and leaves the dead `rlwinm r50` in the block through scheduling (backend-11 still
  has it; the RA deletes it). Pre-RA DAG: the branch now hangs off the `rlwinm.` directly (no cmpi chain), the dead def is a leaf with a kind-0 edge to the
  terminator, and the schedule becomes the target's exactly (variant d: every diff row an ARG_MISMATCH, no INSERT/DELETE). The tree's `(b7 >> 4) & 0xF` compiles to
  one `rlwinm` + `cmpi` (the record form is merged only post-RA) with no leftover. Then the colours (pass 49's RA side): b7 declared between b4 and ofs and kept an own
  local by a second def (`b7 &= 0xF; if (b7 < 1 || b7 > 8) ..; sfcre_mpv_picrate[b7]`, no `picrate_code`) -> 0w (variant g); with the tree's declaration order
  (h) 21w, the test spelling alone (d) 23w = the target's order with every byte register shifted by one (@164 takes r4). `(((b7 & 0xFF) >> 4) & 0xF)` gives the same
  23w as d. Catalogue row (MWCC, "instruction order within a block"): **a mask-then-shift `(x & M) >> k` on a byte (compared with 0) is one scheduler node more than
  shift-then-mask: peephole-forward folds the pair into one `rlwinm` (record form if compared) but the dead mask def stays in the block until the RA. Unlike the
  pass-58 `& 0xFF` index/value masks (deleted by load-deletion / folded before scheduling), this leftover changes the pre-RA schedule.** Flip: objects.py `# CRI
  pass 60` block, locked `ninja -k 0`, `dtk shasum -c` 111 OK.
- **adx_tsvr `adxt_nlp_trap_entry` 2w (`lha r4` vs `r0`): neither lever applies.** B16 (`lha ofst; cmpi n1; lha ofst2v; add`) has no store, no byte index and no
  parameter load; the r0 node the target needs (pass 37/46/49b) must be a level-1 node with a higher vid than the lha temp and live in B16 — only a backend temp
  created later in B16 could be, and B16 has none. Left 2w.
- **sfh_main `SFH_AnlyElemSmpHz` 6w (M4 stwbrx fold): not addressed.** `*(volatile Sint32 *)val = SWAP32(..)` still folds to `stwbrx` (6w, same size), `const
  SFH_ELEM *elem` (a local: t2 class) 6w. The post-RA peephole (backend-18) runs after the RA's dead-def deletion and prologue-epilogue (17); nothing a C
  statement leaves between the last `rlwimi` and the `stw` survives to it. Left 6w.
- **sfd_mps `sfmps_DecodeOneUnit` 5w (cnt r21/r23, +2 ghosts on ret): not addressed.** `const Uint8 *data` is rejected by the CRI prototypes (MPS_CheckDelim /
  MPS_DecHd / sfmps_CopyPketData take `Uint8 *`: four implicit-conversion errors), so the vendor's parameter was not const; the loads of the scan loop go through
  the local `p` (t2 in any case). Left 5w, M1 pin kept.

### CRI SWAR kernels pass 14 (mpv_mc 8x8 H2 494w unchanged in the tree; the target's two rows are the BACKEND'S x2 UNROLL of a one-row loop (body <= 34 pcodes), the `and w0,a0` / `x0 & m1` order is a same-vreg WAR edge (a0 one web, `#pragma opt_lifetimes off` reproduces the target's 26-opcode row order exactly), and the target's colours are ONE level in declaration order with no temp node in the loop; nothing applied, nothing flipped; V2/16x16 items not started; 2026-09-12)
Harness ~/.cache/cri_swar14/ (KEPT, dumps deleted: `mkv.py NAME [--base BODY] [--pre TEXT] 'OLD=>NEW'..` = bodies/NAME.c from the tree's
H2 function with exact-string edits (`\n`/`\t`/`\\` escapes), `p.sh NAME [FUNC] [UNIT]` = variant words (+`RA=1` ra.py dump ra_NAME, `SC=1` scheddump
out_NAME, `NOPRAGMA=1` drops the opt_propagation pragmas), `dis.sh NAME` = clean dtk listing ours_NAME.s (diff against t_h2_8.s / t_v2_8.s / t_h2_16.s =
the target listings), `pc.py RA BLOCK [N]` = the block's before-regalloc pcodes with `vreg=colour[name]` + the block's colouring order, `live.py OUT BLOCK`
= live-count profile of a scheduled block (peak), `case0.py RA` = chaitin.py on the graph with every node of the other cases deleted (a tight-other-cases
what-if), `ast1.py FILE [FROM TO]` = one-line AST statements, `blk.py`). Pass-13 harness deleted.
- **The two-row structure is the compiler's.** `for (i = 0; i < 8; i++) { ROW }` with the tree's case-0 row is unrolled x2 by backend pass 04
  (loop-transforms: ctr 4, both copies with the SAME vregs) and gives the tree's exact 494w/0x40c; the V2 8x8 target (case 0 identical from an
  8-iteration one-row loop) has the same ctr-4/two-store shape, so the vendor's H2 was one row per iteration too. **Threshold (bisected with k extra
  `__dcbt` pcodes on case 0): a loop body of <= 34 pcodes after pass 03 (initial code + peephole-forward + copy/add propagation) is unrolled, 36 is not**
  (case 0 = 30, case 3 = 29 unroll; the tree's case 1 = 36 and case 2 = 39 do not: `u_all` 436w = the pass-2 size). So the vendor's per-case rows had
  <= 34 pcodes INCLUDING the fused-away `rlwinm`s and the peephole `mr`s: case 1's 28 final instructions leave room for <= 6 such pcodes (the tree's
  `__rlwinm(__rlwimi(w0, w1, 0,0,15), 16,0,31)` costs MR+RLWIMI+RLWINM, `__rlwimi(w1 << 8, a1, ..)` RLWINM+MR+RLWIMI).
- **The order residue read with sched.py (out_tree B10, IDENTICAL model):** c7 = `xor x0` + `rlwimi a1'` (the a1 pack beats `and w0,a0` on frees 1/0
  at equal height 19); c8 = `and @120 = x0 & m1` FIRST (frees 2: its srwi + the WAR to `x0 &= m2`) then `and w0,a0` -> a0 and @120 overlap -> 12 live
  -> r31 + frame. `and w0,a0` can never be xor's c7 partner: as a second pick it reads a0 = the GPR written by `rlwimi a0` in the previous cycle (the
  IU forwarding rule blocks it); and at c8 any fresh-vreg `x0 & m1` (expression temp, `t0` local, with `y0 = x0 & m2` separate: frees 1) still outranks
  it (frees > 0 = 0, equal heights: the a0' chain and the w0 chain both reach `stw d[0]`). **schedwhatif `reg=` retargeting `x0 & m1` to a0's OWN
  vreg gives the target's row order at once** (c8 = xor x1, and w0,a0; c9 = and a0', and x0&=m2): the WAR edge `and w0,a0 -> and a0` holds a0' back
  and the target's `and r5,r10,r11` IS a0's register r5 because it is a0's node. The frontend range-splits `a0 = x0 & m1` into @120 (EASS = new web;
  only compound assignments continue a web, and no `a0 OP= e` yields `x0 & m1`: `a0 ^= w0` needs the unmodified w0 that `w0 &= a0` destroyed, the
  identities `a0 += (x0 & m1) - a0` / `a0 = a0 ^ a0 ^ (x0&m1)` are kept as code or folded back to the split). **`#pragma opt_lifetimes off` IS the
  frontend range splitting** (frontend-01 keeps every variable one web; l1) and with it the single-row case 0 (`a0 = (w1 >> 24) | (w0 << 8)`, `a1 =
  __rlwimi(a1, w1, 8, 0, 23)`, in-place average) is the target's 26 opcodes IN THE TARGET'S ORDER (l2, both rows), residue = colours only.
- **Peephole or->rlwimi rules completed (pk_* probes, lifetimes off/on):** the fused operand is the earlier-defined rlwinm EXCEPT when one operand is
  the destination variable: then the destination operand is fused and the other is the base (`a0 = w0 << 8; a0 |= w1 >> 24` / `a0 = (w1 >> 24) | a0`
  / `b0 = w1 >> 24; a0 = w0 << 8; a0 |= b0` all give `mr a0, srwi/b0; rlwimi a0, w0, 8, 0, 23`), and the base is never the destination (`a1 = (w1 << 8)
  | a1` with a1 = the lbz and lifetimes off stays `slwi; or`). So an `rlwimi` INTO a variable's own register with the other operand fused
  (`slwi r5,r4,8; rlwimi r5,r8,8,24,31`) always comes from `mr a0, slwi_t` whose temp got a0's colour; `__rlwimi(a0, ..)` makes `MR t, a0; RLWIMI t`
  and forward-propagates `a0 = t` into the uses (t is the node, a0's web restarts at its next def -> no WAR); `__rlwimi(w0 << 8, ..)` = one backend node
  (the K6 `mr` coalesces backend->backend), no dead rlwinm (lwz w1 then frees 1 -> w0 is loaded first).
- **Coalescing rule (tree -all.txt flags):** backend temp -> @temp coalesces (r131->@119), backend -> backend coalesces (the intrinsic K6 copies), a
  copy INTO or FROM an own local never does (`mr a0, slwi_t` r128 stays a node; `mr @107, a1` r74 stays a node): equal colours by lowest-free are what
  delete those `mr`s.
- **What the target's colours say (read with case0.py on l2 = tight other cases: switch temp r0, lis r4/r5, stride r6, d r3, s r7, w0 r4, a0 r5, w1
  r8, a1 r9, x0 r10, x1 r11, m1 r12, m2 r31 vs target switch r0, lis r4 (0x101)/r5 (0xfeff), stride r3, d r6, s r7, w0 r4, a0 r5, w1 r8, a1 r9, x0
  r10, x1 r0, m1 r11, m2 r12, a0' r5, a1' r9, all four cases alike):** ONE level, colouring = backend temps (switch temp created last -> r0; lis
  temps adjacent to mc's physical r3 -> r4, r5) then own locals in declaration order `d, s, stride, w0, a0, w1, a1, x0, x1, m1, m2` (the loads still
  come out s, d, stride: s's compare chain is the critical path, d/stride tie in input order), stride r3 = loaded LAST (kills mc; r0 blocked by the
  switch temp), d/s adjacent to the lis temps (r4/r5 skipped) and to physical r0 (s, d and the lis temps carry a physical-r0 edge in every dump: the
  entry point). Requirements this imposes: (a) NO @temp and NO backend-temp node in any loop (a temp coloured before the own locals takes r0/r3 and
  x1 = r0 / stride = r3 forbid it; the only loop temps the target allows are the `li 4` ctr temps, r0 each) -> every row value is an own local of one
  node per variable and the pack `mr`s must vanish by equal colours; (b) the loop variables' degree at their scan visit < 29 (level 1) -> the four cases
  add few nodes: the row variables are ONE node each across the four cases (lifetimes off, or webs connected across the switch) — with lifetimes on the
  cases 1-3 webs are @temps (@65..@127 in u0, coloured before the own locals: r0, r3, r4..) which the target's per-case identical colours exclude; (c) l6
  (lifetimes off, four single-row cases, tree op forms) has m1/m2 at 30/29 = level 2 (r0/r3) and 21 leftover backend temps from cases 1-3 coloured
  r4-r6 before the own locals; with the rows tightened both drop out. (d) the one contradiction left: under (a) the a0 pack temp `slwi_t -> mr a0`
  must be coloured AFTER the own locals (a lower level, i.e. own locals >= 29 = lifetimes off cross-case nodes) and must find r0 BLOCKED during
  [slwi, mr] to land on a0's r5 — case 2's target does exactly that (the third word `lwz r0` = x1's first web is live through all four pack slwi's,
  the temps take r5/r4/r9/r8 = the in-place registers), but in case 0/1 nothing coloured r0 is live during the a0 pack (the lbz/lhz is r9 = the a1 pack's
  base, coloured with a1's node: under lifetimes off a1 = {lbz, x1 & m1} is one node adjacent to x1 (r0) -> r9, and `c1 = (w1 << 8) | a1` gets r9 the
  same way = `lbz r9; rlwimi r9`), so the vendor's case-0 a0 pack either had `xor x1` scheduled before the pack `mr` (the lbz loaded before w0: needs its
  chain height >= lwz w0's, not found) or another r0 value live there. Open.
- **Negative this pass (all 494-506w unless noted):** `register a0, a1`; `opt_lifetimes off` on the two-row tree form (500w, 0x424: every hand-copied
  row value is one node -> w0 r31); `x0 &= m2` as `y0 = x0 & m2` (r2: fixes the a0 side of the order by the IU rule — `and a0'` reads x0 written in the
  previous cycle and cannot be the second pick behind `xor x1` — but the a1 side still peaks at 12: `and a1'` is not blocked at c10; 490w/0x3fc);
  `w0 += x0 & m2` expressions (496w); mask variables without the pragma as statements / `register` (506w: propagated to constants, `lis/subi` per case);
  the intrinsic a1 pack `a1 = __rlwimi(a1, w1, 8, 0, 23)` alone (s2, 496w: lwz w0 first, RLWIMI a0 at c5, `and w0,a0` still blocked at c6 by the IU
  rule); `a0 = __rlwimi(w0 << 8, ..); a1 = __rlwimi(s[8], ..)` under lifetimes off (l5: no WAR -> `and a0'` first again).
- Not run: ninja, 111. Tree untouched (src/lib/mpv_mc.c H2 two-row 494w/0x40c, V2 73w; src/lib/mpv_mcy.c H2 197w, V2 225w, 4p 30w); objects.py not
  touched by this pass (another agent's edit is in the working tree). 8x8 V2 (73w), 16x16 H2/V2/4p not started this pass. Next for H2 8x8: write all
  four cases as one-row loops of <= 34 pcodes in the target's op forms under `#pragma opt_lifetimes off` (case 2 first: its `x1 = W(s, 8)` third word is
  the r0 blocker the model needs), read the real RA list with pc.py/case0.py, then hunt the pure-C equivalent of the un-split webs (the pragma is a
  tagged lever) — the 4p 8x8 (Matching) uses range-split pairs, so the pragma would be per function.
- **Level what-ifs on l6's graph (chaitin API, uncoloured never-removed dummies on the own locals to push them to >= 29 at scan 1):** own locals in
  level 2 colour in vid order s r4, d r5, stride r0, w0 r3, a0 r6, w1 r7, a1 r8, x0 r9, x1 r10, m1 r11, m2 r12 (the temps then r3/r6..) — the target's
  x1 r0 / stride r3 / w0 r4 / a0 r5 / d r6 / s r7 / w1 r8 / a1 r9 / x0 r10 would need the declaration order `x1, stride, w0, a0, d, s, w1, a1, x0, m1,
  m2` in that world, and still leaves case 0's pack temp without an r0 blocker; the single-level reading (own locals `d, s, stride, w0, a0, w1, a1, x0,
  x1, m1, m2`, temps first) needs no such order but forbids every loop temp node. Neither is reached by a spelling found this pass; the decisive probe
  for the next pass is case 2 alone (its r0 blocker exists): one-row loop, lifetimes off, `x1 = W(s, 8)` third word, pack forms of <= 34 pcodes, read
  with pc.py whether the four pack temps take r5/r4/r9/r8.
- Unroll fact for the catalogue (MWCC row "bl vs inlined body / block splitting" neighbour): **backend pass 04 unrolls a counted loop x2 when its body
  has <= 34 pcodes after pass 03 (36 does not); the copies share every vreg, so a variable's webs of the two copies are ONE node and the second copy's
  loads wait on the first copy's stores/dcbt barrier only** — a hand-unrolled two-row body (fresh @temps per row) gives the same words here but not the
  same graph. Case-0 rows of 30 and case-3 rows of 29 pcodes unroll; the tree's case 1 (36) and case 2 (39) rows do not (u_all: 436w = the pass-2 size).
- **mpv_umc `mpvumc_OneReadMb` 48w: lever (a) is negative and the target says why.** `const MPVUMC_RFB *rfb` 86w (size -4: the second `rfb->ypitch` read is no
  longer blocked by the `stw ofs[0]` and is merged into the first — the target HAS the `lha ypitch` reload after the store, so the vendor's rfb loads were in the
  stores' alias class), `const MPV_MV *mv` 68w (the vx/vy loads hoist above the `ofs[]` stores to the block top; the target loads them AFTER the stores, at c22),
  both 102w, plus `const MPVUMC_OBJ *mpv` 102w. Rule: **a load that the target keeps below an unrelated store, or a reload the target keeps after a store, proves
  the vendor's pointer was NOT const** (the const record would free it). No byte index / `<< 24` value in the block (`& 1` masks on Sint32 vectors). Left 48w.
- **mwsfdcre `mwPlyCalcWorkSfd` 4w (the `size` second read): 20 more spellings, none codeless.** The frontend pulls the single-rvalue-read web into the read in
  every form: `register size` + `return sibsiz + size` 13w, `(void)size;` 13w, `(size, size)` 13w, `size2 = sibsiz + size; return size2` 13w, `return sibsiz +
  (size += K)` 13w, trailing harmless defs (`size = size2; size2 = size`, `+1/-1`, `(Uint8 *)` round trips) 14w, self-cancelling second reads (`size - size`, `size ^
  size`, `size & 0`, `| (size & 0)`, `0 * size`) folded 13w/4w, constant-condition `?:` folded 13w. Two informative ones: `size = sibsiz + size; return size;`
  (EASS + return) 5w — the epilogue `lwz r0` is NO LONGER hoisted (a new web substituted into `@ret` = no coalesced copy deleted in the block, the target's shape)
  but the chain is still pulled and reassociated (`add r3, r0, r29; addi 0x4800`); `return sibsiz + (size > 0 ? size : size);` 1w — an identical-arm `?:` on
  `size` IS a second read (no pull, `add r3, r29, r3` and the epilogue exact) but the condition's compare survives as `addic. r3,r3,0x4800` (`size ? size : size`
  3w +4, `size >= K` 1w +4, a global/other-variable condition +4..+12). `size++` in the return (dead increment deleted) blocks the pull too but the chain is re-
  associated differently (10w). The codeless neighbour pin `asm { mr r11, size; mr size, r11 }` blocks the pull (the frontend sees the asm read) but its RA-
  deleted copies dirty the block (`lwz r0` hoisted) and the r11 ghosts shift the accumulator to r5 (15w). So the vendor's second read of `size` was codeless AND
  left nothing the RA deletes: still open (a read in a `?:` whose compare is deleted, or a construct not yet seen). Left 4w.
- **mwsfdcre `mwsfcre_CreateSfd` 115w (the dead `b T` of `case 4:`):** a codeless neighbour pin in the case-4 arm (`asm { mr r11, mode; mr mode, r11 }` with a
  `register` parameter or a local copy) does NOT leave the `b` (size still 0xe68; the r11 reservation costs 276w): an arm whose only content is RA-deleted copies is
  dropped with its branch, so the vendor's arm content was deleted EARLIER than the RA but after the frontend (backend CSE / constant-propagation / load-deletion
  of a live-looking statement) — consistent with pass 51's "redundant with a dominating def". `mode` is a `lwz` (not a byte), so lever (b)'s mask is not deletable
  here. Left 115w (2 dead `b`).
- **Not tried this pass:** nothing else. Tree edits: src/lib/sfd_cre.c (AnalyMpv declarations, test spelling, b7 second def, header comment), config/G4BE08/objects.py
  (`# CRI pass 60` block). sfd_cre IDENTICAL, `ninja -k 0` + `dtk shasum -c` 111 OK. Harness ~/.cache/cri60 deleted.

### CRI pass 59 (cftfx UserTable 73 -> 2w in the harness, not yet applied: the rows-3/4 level-1 slot = the `|=` pack spelling; in progress; 2026-09-12)
Harness ~/.cache/cri59/ (`mk.py`, `v.sh NAME [BLK]` = variant words + scheddump + `chk2.py` = the 7 target pre-RA order facts
identified by ROLE (y0..y3, t0..t3, slwi/rlwimi/oris 0/1, d0/d1, add, subf) so vreg renumbering does not matter; `search.py` = sched.py on
B5 with invisible pcodes (coalesced copies / dead defs) inserted at every chain point, singles/pairs/triples x input orders; deleted at the end).
- **Why the slot could not move (search.py):** in B5 `lbz y2` frees TWO successors at c5/c6 (`lbzx tbl[y2]` AND the `addi y,y,4`, whose preds are
  the four index loads and y2 is the last one issued), so no input order, alias change or single dead def lets `lwz d0` (frees 0, h10) or
  `lbzx tbl[y3]` (frees 1, h9) beat it; the only DAG edits that give all 7 facts are a +1-cycle delay on pack 0's `mr v0` AND +1 height on `lwz
  d0` (pairs: copy after `slwi B0` / after `mr v0` / a dead READ of v0 between `mr` and `rlwimi`, each with a copy after `lwz d0`).
- **The C: `v0 = (A << 24); v0 |= (B << 8);` (two statements per pack) instead of `v0 = (A << 24) | (B << 8)` (b1: 73 -> 11w, all 7 facts).**
  The or->rlwimi peephole (pass 01) still inserts the earlier-defined shift (`rlwimi 24,0,7`, same shape) but the `<<24` rlwinm now WRITES v0's
  register (it is v0's first def) and becomes a dead def that is deleted only at RA: at scheduling time it is a WAW predecessor of the peephole's
  `mr v0, B` (kind 0), and it is ready only when tbl[y0] arrives (c5), so `mr` moves to c6 (same-GPR IU rule), `rlwimi` to c7, `oris` to c8:
  `lwz d0` takes the LSU at c6 (before the rlwimi -> tbl[y0] r31), `lbzx tbl[y3]` at c7 (before the oris -> y3 r6). Row 3 (B4) gets its two facts
  the same way. Rows 1-2 unchanged (their colours never depended on the slot).
- **Then p3'/pack colours (b2: 11 -> 2w):** with the packs no longer volatile in B5, `@164` (p3's second web) and the row-4 pack webs compete for
  r26: level 1, descending vid, and the range-split webs are numbered per variable in FIRST-DEF order with the vids descending along the @ numbers,
  so `p3 = p2; p4 = p2;` must come BEFORE row 1 again (p3's webs numbered before v0/v1's = higher vid = coloured first = r26, packs r27); pass
  58's "after row 1" placement was only right while the packs were volatile.
- Left (2w): setup `slwi r0,r11,2` (@155 hoisted `ywidth*4`) before `add r30` (yskip) in the target, after it in ours: a pre-RA c9 tie (both h1,
  urgent, frees 0) resolved by input order — the hoisted @temps are appended after the for-init `li i,0`, yskip's add precedes it; the post-RA
  run of B0 keeps the order (c11 tie, input order again).
- **APPLIED to src/lib/cftfx.c (73 -> 2w, 5/6, size 0x22c exact, not flipped):** `CFT_A256_ROW` packs as `v = A << 24; v |= B << 8;`
  and `p3 = p2; p4 = p2;` before row 1. Unit built with the locked ninja, bytecmp 2 words (0x48/0x4c = the setup swap), objects.py untouched.
- **The last 2 words, read with the post-RA replay (`b0post.py`/`b0lib.py`/`b0pairs.py` in the harness = pre-RA B2 under schedwhatif edits ->
  physical registers with the tree's colours -> post-RA schedule of the merged entry block -> diff against the target's setup):** ours'
  pre-RA output with ONLY `rlwinm @155` moved before `add yskip` reproduces the target's entry block exactly, so the target's pre-RA B2 had the
  slwi before the add. Statement permutations of the setup (6 groups, all valid orders) never do it (the c9 tie is by input order and the
  hoisted @temps always follow the for-init); deletions (the dead `<<4` of dskip, the `li i`, the y/d loads) never do it; a single invisible
  pcode never gives the target's whole entry block (it shifts the completion queue and moves the mulli/loads); exactly these PAIRS of
  invisible pcodes give 0 differing slots: a coalesced copy after the `mulli ywidth*3` + any early IU filler (a dead read of width / the
  wblk-hblk srawi-addze / height / ywidth, or a copy after the dskip `srawi`/`addze`), or a copy after `subf ywidth-width` + a dead read of
  the mulli, or a copy/dead read of @155 itself + a dead read of the subf. Spellings tried and negative (all still 2w or worse): yskip
  expression in the loop (134w), yskip assigned after `i = 0` with `for (;;)`, two-def own-local `yw4` (folded back into the hoisted @temp,
  both `yw4 = yw4 * 4` and `*=`), `yskip = ywidth * 3; yskip = yskip + ..` (in-place add, 3w), dskip as `/4` + `* 16` at the use or `/4*4*4`,
  a `width` local, `v0 = 0, v1 = 0` initialisers (deleted by the frontend, and they renumber the webs: 11w). Next: find the setup value
  whose def is a range-split/argument-style COMPILER copy of `ywidth * 3` (or of `ywidth - width`) plus one more dead setup def; the model
  answers in 0.1 s per candidate (`python3 b0lib.py` API: `evaluate([edits]) -> (slwi_first, post_RA_diff)`).
- Harness ~/.cache/cri59/ kept SMALL (scripts, base.c/tree.c, the b2 dumps; obj_*/ra_* deleted); ~/.cache/cri58 deleted.

### CRI pass 61 (cftfx 5 -> 6/6 FLIPPED, 111 OK: cnvDynamicYcc420plnToA256UserTable 2 -> 0w pure C — the setup `slwi r0` (ywidth*4) / `add r30` (yskip) tie is input order, so the two strides became OWN locals declared first; no invisible pcodes needed; 2026-09-12)
Harness ~/.cache/cri61/ (cri59's scripts + `b2.sh NAME` = variant words + the pre-RA INPUT and scheduled order of B2 in one line each; deleted at the end with ~/.cache/cri59).
- **The form (APPLIED, IDENTICAL):** declarations `Sint32 w4; Sint32 dskip;` FIRST (before p4), `Sint32 yskip;` uninitialised after p2, then the statements
  `w4 = ywidth * 4; yskip = ywidth * 3 + (ywidth - dst->width); dskip = (dst->pitch - dst->width) / 4 * 64;` before the loops, `y = p3 - w4 + 4` in the
  inner tail and `d = (Uint32 *)((Uint8 *)d + dskip)` in the outer tail (dskip in BYTES). Pre-RA B2 input: `.. lwz buf | rlwinm w4 | mulli | subf | add
  yskip | lwz pitch | subf | srawi | addze | rlwinm dskip,6 | li i | b` -> scheduled `.. rlwinm w4 | add yskip | rlwinm dskip | b` = the target's block
  (b0lib replay: 0 differing slots for slwi placed anywhere before the mulli with the dskip chain before the `li` and no dead `<< 4`).
- **Why the pass-59 pairs were a detour:** the tie IS resolved by input order, and a frontend-hoisted @temp is always appended after the for-init, so
  the only way to put `ywidth * 4` before yskip's add is an own-local STATEMENT before yskip's (h19: the same statement after yskip's = 2w again). The
  own local then has to be coloured FIRST (target r0 = the first colour of the whole loop nest: every loop value overlaps it and none is r0): own
  locals are coloured in declaration order inside their level, but the hoisted @temps (`@155` ywidth*4, `@156` dskip's `<< 6`) sit above every own
  local, so w4 declared anywhere (h2/h3, 88w) lands behind dskip's @temp (r3 instead of r0, the whole nest shifts). Hence dskip must ALSO be an own
  local, declared right after w4 (h20 with dskip before w4 = 5w, the r0/r3 swap; h18 with the folded dskip = 5w).
- **dskip's @temp, read (h9/h10/h14):** `d += dskip` with `Uint32 *d` and a single-def `dskip = E * 16` is algebraically folded by the frontend into
  `d + (E << 6)`: a NEW expression at the use, hoisted to after the for-init as `@156` with the whole `lwz pitch / subf / srawi / addze` chain
  re-created there and a dead `rlwinm E,4` leftover (the tree's r68); dskip's own def disappears (declared last) or stays as a dead own-local def
  (declared first, h9). Declaration order of dskip vs yskip changes nothing (h10 = the tree's dump). In bytes (`/ 4 * 64` + a byte-pointer add) no fold
  happens: dskip is an own local at its statement position (h14) and there is no dead `<< 4` node — the entry block still schedules the same.
- **Negative this pass:** helper `@ret` / argument copies as the "coalesced copy after the mulli" (`static Sint32 cft_mul3(Sint32 w) { return w * 3; }`
  4w, with a helper-local `ret` 4w: the copy is propagated before scheduling, and the inlined body's mulli is even emitted after the subf; a two-argument
  `cft_add(ywidth * 3, ywidth - width)` 101w: no copies survive, the add becomes the helper's temp r58); `yskip = ..` as a statement inside the outer loop
  (h7: the parts are hoisted as @temps, the add stays in the loop, 134w); `d += (dst->pitch - dst->width) / 4 * 16` in the loop (h15: only the loads
  are hoisted, 134w). The b0pairs search (copy/dead-read pairs on B2) reproduces pass 59's list; none was needed.
- **Catalogue row to add (MWCC table, next to "instruction order within a block"):** a setup value the target computes BEFORE a statement that precedes
  the loop, while ours computes it from a hoisted loop invariant (`slwi` of a stride used only in the loop body) | frontend hoisting appends the loop's
  invariant @temps after the for-init, in creation order; a pre-RA tie between two independent IU ops is input order | make the invariant an own-local
  statement placed before the statement it must precede; declare it (and every other former @temp of the same level, e.g. a byte-scaled `dskip`) FIRST
  so its colour stays the @temp's (r0: highest vid of the level); keep a pointer step in bytes so `ptr += step` is not folded into a new hoisted `<< k`
  @temp | — | "CRI pass 61".
- Flip: objects.py `# CRI pass 61` block (`"lib/cftfx.c": True`), locked `ninja -k 0`, `dtk shasum -c config/G4BE08/build.sha1` 111 OK. Harnesses
  ~/.cache/cri59 and ~/.cache/cri61 deleted; the kit untouched.

### CRI SWAR kernels pass 15 (mpv_mc 8x8 H2: the x2-unroll threshold is <= 35 pcodes and all four one-row cases reach it under `#pragma opt_lifetimes off` (case 2 with `__rlwimi(w << k, v, ..)` temp packs); the target's colours are reproduced by the allocator model ONLY with no pack-temp node in any loop + `x1` coloured first, and every C pack spelling under the pragma leaves a temp node — nothing applied, nothing flipped (H2 494w, V2 73w, mpv_mcy H2 197w / V2 225w unchanged); 2026-09-12)
Harness ~/.cache/cri_swar15/ (KEPT without dumps: `h2gen.py SPEC` = bodies/NAME.c from a spec with `@pre/@decl/@rowK/@loopK/@post`
sections (one-row `for (i < 8)` loop per case by default), `t.sh SPEC` = variant words + ra.py dump + `cnt.sh` (pcodes per loop block after
pass 03 / 04: doubled = unrolled), `rows.py OURS.s TARGET.s [CASE]` = register-blind per-case loop opcode diff, `wi.py RADIR [--drop-loop-temps]
[--order n1,n2,..]` = chaitin.py what-if (delete every backend temp that lives only in loop blocks / permute the own locals' vids), `pc.py`,
`mkv.py`/`p.sh`/`dis.sh` from pass 14 with the path changed; specs base/c2a/D1-4/E1-2/P1-3/Q1-2/d4-d10 and the target listings t_*.s).
Pass-14 harness ~/.cache/cri_swar14 DELETED.
- **Unroll threshold corrected: a counted-loop body of <= 35 PCodes after backend pass 03 (the `LOOPWEIGHT` line not counted) is
  unrolled x2 by pass 04; 36 is not** (d4-d10 = case 0's 29 + k `__dcbt`s: 33/34/35 -> 56/66/68 (doubled), 36..39 -> 35..38). Pass 14's
  "<= 34 / 36" counted the LOOPWEIGHT line. Consequences: the tree's case-1 row as a ONE-row loop is 35 and unrolls as it stands (B14 35 ->
  68); case 2 is 39 (four or-packs = 4 pcodes each: rlwinm, rlwinm, `mr own, base`, rlwimi) and needs <= 12 pack pcodes for its 8 final
  pack instructions: the four intrinsic temp forms `a0 = __rlwimi(w0 << 24, w1, 24, 8, 31); w0 = __rlwimi(w0 << 16, w1, 16, 16, 31); a1 =
  __rlwimi(w1 << 24, x1, 24, 8, 31); w1 = __rlwimi(w1 << 16, x1, 16, 16, 31)` (3 pcodes each: rlwinm, K6 `mr`, rlwimi; no fused rlwinm) give
  35 -> 68 with the target's 29-opcode multiset (order residue: the second `slwi 16` + the xor/and group, c2a). Case 3 = 29. So `c2a` = all
  four cases one-row x2-unrolled under the pragma (505w, size 0x43c vs target 0x3ec: every pack `mr` kept + a 2-register frame) — the
  count budget per row is final instructions + `addi d` + `addi i` (the ctr counter's increment IS a body pcode until pass 04) <= 35.
- **The physical-register edges read (base/c2a `-all.txt`):** a node gets an `r0` neighbour iff it is used as the rA/base of a D-form
  load/store/`dcbt`/`addi` (s, d, both `lis` temps: `addi m, lis, K`; stride (only an rB), the switch temp and every loop value have none);
  an `r3` neighbour iff live while `mc` is (s, stride, lis temps, switch temp; d's load kills mc). So the target's `lis r4 (0x101), lis r5
  (0xfeff)` = the two lis temps coloured after the switch temp (r0) with r0/r3 blocked, m2's lis (created later = higher vid) first.
- **What the target's colours require, by chaitin.py what-if (wi.py on c2a, model IDENTICAL to the compiler on every dump):** deleting
  every backend temp node that lives only in the loop blocks (= "every pack `mr` coalesced") makes the whole graph ONE level (s/d/stride/m1/m2
  fall from degree 33-37 to < 29) and the own locals colour in declaration order: switch r0, lis r4/r5, then `x1` FIRST (r0), stride r3
  (must be loaded LAST: its load kills mc), w0 r4, a0 r5, d r6, s r7, w1 r8, a1 r9, x0 r10, m1 r11, m2 r12 = the target exactly
  (`--order x1,stride,d,s,w0,a0,w1,a1,x0,m1,m2`; with x1 declared last it takes r12 and w0 slides to r0). The ctr `li 4` temps stay
  (r0, adjacent to s/d/stride/m1/m2 only). Any pack temp left as a node is coloured BEFORE the own locals (higher vid) and takes r4-r10, and
  pushes s/d/stride/m1/m2 to level 2 (r0, r3, r4, r5, r6 first) — base 479w, c2a 505w, D1-D4 (four declaration orders incl. x1 first) 506w.
- **No C pack spelling under `opt_lifetimes off` is node-free (probes P1-P3, Q1-Q2, all read at pass 03):** `a0 = w1 >> 24; a0 |= w0 << 8`
  -> `rlwinm a0 (dead); rlwinm t; mr a0, t; rlwimi a0, w1, 8, 24, 31` (the case-0 shape, t a node); `register Uint32 a0` identical
  (register locals only get the LOWEST vid = coloured last); `a1 = (w1 << 8) | a1` / `a1 |= w1 << 8` with a1 = the lbz stay `slwi; or`
  in the FINAL code (no rlwimi: the base may not be the destination); `__rlwimi(a1, ..)` = `mr t, a1; rlwimi t` (t the node); a union
  bitfield insert goes through the stack (stw/stb/lwz). Copies into or from an own local never coalesce; only backend->backend (K6) and
  backend->@temp do. With `opt_lifetimes on` (E1/E2, one-row loops): ~43 @temps, s/d/stride/m1/m2 degree 62-66 -> level 2 -> r0/r3/r4/r5/r6
  (498w). Hence the target's graph = one web per row variable ACROSS the four cases (only the pragma does that) AND a pack whose base
  copy is coalesced (only an @temp destination does that) — a combination no spelling produced. Remaining mechanism for H2: how the vendor's
  packs put `slwi/srwi` into the variable's own register without a copy node (a frontend that keeps one web AND treats the variable as a
  coalescable @temp — e.g. an unrecognised pragma/keyword, or a codegen path emitting RLWIMI onto the variable directly).
- **8x8 V2 (73w, cases 1-3 colours/order):** the target's case-1 `lbz r31, 8(s1); mr r28, r31; .. rlwimi r28, r30, 8, 0, 23` is a KEPT copy
  from an own local a2 (the s1 byte) while the s0 byte is substituted (`lbz r29; rlwimi r29`): in the tree both bytes are single-use and
  substituted (coalesced, no `mr`). `s1 += stride` between `a2 = s1[8]` and its use keeps a2 an own local (V3: `lbz r33[a2]`, `mr @191, a2`),
  but a2 is coloured r28 = @191's colour and the `mr` vanishes (73w unchanged); the target's a2 = r31 needs r28/r29/r30 blocked = a2 adjacent
  to a1' and the w2 pack in the pre-RA order (a later second use of a2, or a2 loaded after them). Load statement order (V1/V2) changes
  nothing (loads issue by height). Not closed.
- **The one-row/x2 reading does not apply elsewhere:** the 8x8 V2 target is ctr 4 only in case 0 (32 pcodes, unrolled, identical) and ctr 8
  in cases 1-3 (48/48/50 pcodes); mpv_mcy 16x16 H2 and V2 are ctr 16 with bodies of 73-90 / 62-96 pcodes (one row per iteration, never
  unrolled), so their residues (197w / 225w) are colours/order only.
- Tree untouched (src/lib/mpv_mc.c H2 two-row 494w/0x40c, V2 73w; src/lib/mpv_mcy.c H2 197w, V2 225w, 4p 30w); objects.py untouched; no
  ninja, no 111. Next for H2: find what makes a pack's base copy coalesce into a one-web variable (test `#pragma opt_lifetimes off` on the
  4p Matching function to see whether the vendor could have had it file-wide; probe the `-O4,p` vs `-O4` and `opt_common_subs` pragmas'
  effect on the `mr own, t` copies with wi.py as the judge before touching the rows).

### CRI pass 62 (adx_baif Matching 5 -> 6/6 FLIPPED, 111 OK: AIFF_GetInfo 170 -> 0w pure C — the `long`-vs-`int` compare quirk keeps a single-use local, a `(Uint16)` cast on a duplicated macro argument breaks the frontend CSE; adx_dcd5 below; 2026-09-12)
Harness ~/.cache/cri62/ (probe.sh = variant.sh on a body file, mkh.py; deleted at the end). Tree: src/lib/adx_baif.c AIFF_GetInfo
body + comment, config/G4BE08/objects.py `# CRI pass 62` block. Locked `ninja -k 0`, `dtk shasum -c` = 111 OK.
- **AIFF_GetInfo 170 -> 0w, four source shapes, all read off the dumps in ~35 min (each one a 1-second probe once the mechanism was named):**
  1. **The kept header `ckid` (target `mr r27,r30; rlwimi r27,r31,24,0,7` = an own local written by the last OR; ours substituted it into the
     compare):** the vendor's `ckid` is `Sint32` (= `long`). MWCC types `long != int-constant` by converting the LONG operand to `int`, and it does
     so by RETYPING the indirection: frontend-00 shows `ENOTEQU int (EINDIRECT int (EOBJREF [ckid] pointer(long)), EINTCONST int)` against the
     def `EASS long <- ETYPCON long (EOR unsigned long ..)`. The frontend's single-use substitution requires the use's EINDIRECT type to equal
     the object's type, so the `int` read of a `long` object is never substituted: the variable is kept whatever the block structure. Probes:
     `Uint32 ckid` (169-170w, substituted), `int ckid` 169w, `Sint32 ckid` vs `(Sint32)AIFF_FORM` 169w, `(Sint32)ckid != AIFF_FORM` on a Uint32
     169w — ONLY `long` object vs `int` constant keeps it (49w). Arithmetic does not retype (`hi & 0xFFFF` on a long = `EAND long` with the
     constant converted; h18 propagated). `ssnd_flg != 0` on the tree's `Sint32 ssnd_flg` shows the same `EINDIRECT int` read.
  2. **`end`: target `subi r10,r12,4; add r10,r8,r10`, ours `add; subi`:** `end = p + (cksz - 4)` is codegen'd `add; addi -4` (the backend
     reassociates pointer + (int + K)); `end = p + cksz - 4` gives the target (47w). `&p[cksz-4]`, `cksz -= 4`, `(Uint32)` casts: no change.
  3. **The 16-bit reads stored to `*nch`/`*bps` (`clrlslwi r12,r12,16,8; rlwimi r12,r29,0,24,31`):** the OR's operands are BOTH masked values
     and the or->rlwimi peephole keeps the rotate-and-mask one as the base: `(p[0] & 0xFF) | ((p[1] & 0xFFFF) << 8)` (h22, 30w). Without the
     `& 0xFF` on the low byte the base is the plain lbz and the 16-bit mask is folded into `rlwimi 8,16,23` (h23 = the tree). backend-01 shows
     peephole-forward merging `rlwinm 0,16,31; rlwinm 8,0,23` into `rlwinm 8,8,23` (= clrlslwi 16,8) and narrowing it to 16..23 only when it
     folds it into an rlwimi (it uses the lbz range there, not before). Negatives (47w, propagated/folded): Uint16/Sint16/Sint32/int locals for
     hi/lo with one use each, `(Uint16)(int)hi`, `(hi + 0)`, `Sint8 *`/`char *` views (extsb appears), the mask on all four reads (57w, +0x10:
     exp/mant use the clean form).
  4. **exp/mant (`clrlwi r27,r31,16` / `clrlwi r28,r12,16` write callee-saved registers = kept own locals; the COMM block hands out r29/r30/r31
     before the loop's byte temps):** `Uint16 exp, mant` (kept: the `(Uint16)` conversion writes the variable) = 27w, and the loop/COMM colours
     need the LE16 value under SWAP16 to be a BACKEND temp (high vid, coloured in its own statement region: mant lo -> r30, exp swap -> r31,
     exp lo -> r29 new, then bps/nsmpl/nch lo bytes reuse r29) instead of the frontend's CSE @temp (low vid, coloured last): `exp =
     SWAP16((Uint16)(p[8] | (p[9] << 8)))` — the `(Uint16)` cast on the duplicated macro argument stops the frontend CSE (pass 13's "casts break
     the CSE"), the backend's load-deletion/CSE merges the duplicate into one value used twice. `(Uint32)`/`(Sint32)` casts do NOT break it (25w),
     `& 0xFFFF` gives +8 bytes. IDENTICAL with either `exp = SWAP16((Uint16)..)` or `(Uint16)SWAP16((Uint16)..)`.
- Colouring facts confirmed in chaitin.py (order/colours exact on this graph; the model's cost column differs for rlwimi read-write operands,
  irrelevant at level 1): all nodes are level 1, so the colouring order is plain DESCENDING vid = reverse statement order; a callee-saved
  register once handed out is preferred over a new one and the handed-out set is tried in ASCENDING number (r29 before r30 before r31), so
  which statement region first needs a third callee-saved register decides every later temp colour in the function.
- Catalogue additions (MWCC table): row 2 (kept copy / substituted single-use local) — a `Sint32` local compared with an `int` constant is
  read as an `int` indirection and never substituted (`Uint32`/`int` locals are); row "expression computed once/twice" — a cast on a macro
  argument that the macro duplicates (`SWAP16((Uint16)x)`) turns the frontend @temp into a backend temp (colour rank: coloured with its
  statement's temps, not last); row "two values with swapped registers" — the handed-out callee-saved set is tried in ascending order.

### CRI SWAR kernels pass 16 (mpv_mc 8x8 H2: the pack copy DOES coalesce when the row variables are the locals of an inlined `static inline` helper and `#pragma opt_lifetimes off` is on the CALLER — case 0 is the target's 49 opcodes in the target's order with no temp node; the allocator model gives the target's colours from that graph with the helper declaration order `m2, m1, x0, a1, w1, s, d, a0, w0, stride, x1`; cases 1/2 still leave K6 nodes / exceed the 35-pcode unroll budget; nothing applied, nothing flipped (H2 494w, V2 73w, mpv_mcy H2 197w / V2 225w unchanged); 2026-09-12)
Harness ~/.cache/cri_swar16/ (cri_swar15's scripts with the path changed; `h2gen.py` gained `@func` = static inline helper name (the public
function then goes in `@post`) and `@sw` = the switch expression; `wi.py --at` treats `@N` webs as named nodes (`--order @63,@71,..`); `pc.py` reads
backend-10; specs h1a/h1b/h1c/h4a-c/h5/h6a-b/h7 + bodies/tree.c; DELETE it at the end of the next pass). Pass-15 harness deleted.
- **H1 (single-def row values, lifetimes on; h1a 435w): confirmed for @temp webs, not for own locals.** Cases 1-3's row webs are @temps and their pack
  bases coalesce into them (ghost list `r47->@87 ..`); case 0's first webs are own locals: `mr a0, t` stays a node (deleted only by the equal colour
  r7). Every lifetimes-on form (h1a, h1c with `x1 = (Uint32)s & 3; switch (x1)` = the switch temp as x1's first web, own-local x1 r7 8/8) has
  s/d/stride/m1/m2 at degree 65-101 -> level 2 (r0/r3/r4/r5/r6): ~40 per-case @temps. The target's graph must have ONE node per row variable
  across the four cases (pass 15 stands). `#pragma opt_lifetimes off` written around a `static inline` helper's DEFINITION has no effect on the
  inlined copy (h4b-1 = h4a byte for byte); it must enclose the caller.
- **The coalescing rule completed (h4b/h4c/h5 dumps): the RA coalesces `mr X, Y` only when X (the destination) is a backend temp or an `@N` web
  (helper locals ARE `@N` webs); a copy whose destination is an own local never coalesces, and a copy FROM an `@N` web into a backend temp (the K6
  `mr t, w0` of `__rlwimi(w0, ..)`, `mr t, a1` of `__rlwimi(a1, ..)`) does not either (h4c B14: r60/r61 stay nodes r7/r6).** So `a0 = (w1 >> 24) |
  (w0 << 8)` with a0 a helper local under lifetimes off gives `slwi a0, w0, 8; rlwimi a0, w1, 8, 24, 31` with NO node (r56 -> @67 coalesced;
  the fused `srwi` leftover r55 is a 0-degree dead def), and `a1 = __rlwimi(a1, w1, 8, 0, 23)` with a1 = the lbz gives `lbz a1; rlwimi a1` the
  same way (r57 -> @65). h4c/h5 case 0: `rows.py` order diff 0 lines (the target's 49 opcodes in the target's order, registers only).
- **Helper-local vids ascend in DECLARATION order (first declared = lowest = coloured last), the reverse of own locals**: h5 decl `s, d, stride,
  w0, a0, w1, a1, x0, x1, m1, m2` -> r39 @71 s .. r49 @61 m2 (the frontend numbers the webs @71 downwards in the same order). With the intrinsic
  K6 results the BACKEND copy propagation (pass 02) replaces the variable's reads by the temp (`xor .., r63, r61` for `w1 = __rlwimi(w1 << 8,
  ..)`), so an intrinsic-assigned variable's pack is a node whatever the web kind; the or-pack's `mr @67, t; rlwimi @67` is not propagatable
  (the rlwimi modifies @67) and coalesces.
- **The model on h5's graph (`wi.py ra_h5 --at --drop-loop-temps --order @63,@69,@68,@67,@70,@71,@66,@65,@64,@62,@61`, i.e. every loop temp
  NODE deleted, ghosts kept, vid order x1 > stride > w0 > a0 > d > s > w1 > a1 > x0 > m1 > m2) = the target EXACTLY: switch r0, lis r4/r5, x1 r0,
  stride r3, w0 r4, a0 r5, d r6, s r7, w1 r8, a1 r9, x0 r10, m1 r11, m2 r12.** Without the deletion the trio and m1/m2 are level 2 (m1 @62 30
  neighbours: 9 variables + m2 + switch + lis + 4 ctr `li` + 14 loop ghosts/temps). Budget read off it: m1/m2 are visited FIRST in the scan
  (lowest vids) with nothing removed, so 16 + G < 29 -> at most 12-13 loop ghosts in the whole function = exactly the target's coalesced copies
  (case 0: 2, case 1: 4, case 2: 4, case 3: 2 = 12). Ghosts count like nodes (a hand-unrolled two-row case doubles them: h7 = h5 with cases 1/2
  as `for (i < 4) { ROW ROW }` -> degrees 38-42, level 2 again, 494w) -> the vendor's rows were one-row loops unrolled by the backend, and
  every pack copy of every case coalesced (no node), and the helper's declaration order was `m2, m1, x0, a1, w1, s, d, a0, w0, stride, x1`
  (any position for `i`). A level-2 reading is excluded: m1/m2 last (r11/r12) needs them below the row variables while m1 is adjacent to a
  superset of every row variable's neighbours.
- **Left for H2 8x8 — case 1's `rlwimi w0, w1, 0, 0, 15; rotlwi w0, 16` and `rlwimi lhz, w1, 16, 0, 15` in place with no node, and case 2's
  four packs at <= 35 pcodes:** the or-pack costs 4 pcodes after pass 03 (rlwinm base, dead fused rlwinm, `mr`, rlwimi) -> case 2 = 39 (h5
  B18, not unrolled; the dead def and the `mr` both count: 39 - 4 would have unrolled), the intrinsic `__rlwimi(w0 << 24, ..)` costs 3 but its
  result is a propagated node. Probes negative: `w0 <<= 16; w0 |= w1 >> 16` (the destroyed-source shift is not fused: `slwi; srwi; or`, h6a
  37 pcodes), `a0 = w0; a0 <<= 24; a0 |= w1 >> 8` (copy-propagated to `rlwinm a0`, then a0's own def is the fused operand: `srwi t; mr a0, t;
  rlwimi a0, w0, 24, 0, 7`, h6b 35 pcodes unrolled but wrong shape), `__rlwimi(H(s, 8), w1, 16, 0, 15)` = lhz + K6 on a LOAD coalesces
  (backend->backend) and is the right form for case 1's a1. Count of the target's case-2 row: 29 final + `addi d` + ctr `addi` = 31, so its
  four packs cost <= 4 extra pcodes in total: a pack form with the base rlwinm written into the variable's register and the fused shift not a
  separate pcode (or no `mr`) is still to be found; the `-inline auto` inlining of a ~120-statement helper also needs `#pragma inline_max_size`
  in ours (the vendor's helper was inlined without it, or was smaller).
- **H2/H3 not run** (the box went to the helper-local finding): the same-vreg WAR order residue of pass 14 is moot once a0 is one `@N` web
  (h4c/h5 case 0 = target order with lifetimes off); left-to-right vs right-to-left association only matters for 3+ operand packs (16x16).
- **8x8 V2 case 1 (73w) and 16x16 H2/V2 colours: not started this pass.** For V2 case 1 the reading above applies: the target's kept `lbz r31
  (a2); mr r28, r31; rlwimi r28, r30` is a copy INTO a helper-local/@temp web from an own local or a byte value that stays live (the `mr` is
  deleted only by equal colours; a copy into an own local is a node, a copy into an `@N` web coalesces) — test the V2 body as the same helper
  form first. For the 16x16 (ctr 16, no unroll) use `wi.py --at` on a helper-form dump with `--order` to read the declaration order that gives
  the target's r17-r29 handing.
- Tree untouched (src/lib/mpv_mc.c H2 494w/0x40c, V2 73w; src/lib/mpv_mcy.c H2 197w, V2 225w); objects.py untouched; no ninja, no 111.
  Catalogue row to add (MWCC table, "parameter copy kept" neighbour): a pack/shift written straight into a VARIABLE's register in the target
  (`slwi a0, w0, 8; rlwimi a0`) while ours keeps `mr own, t` | the RA coalesces a copy only into a backend temp or an `@N` web (range-split
  web, CSE temp, INLINED HELPER LOCAL); never into an own local, never from an `@N` web into a backend temp | put the kernel body in a
  `static inline` helper (its locals are `@N` webs, vids in declaration order) and, for one web per variable across the switch cases,
  `#pragma opt_lifetimes off` around the CALLER | — | "CRI SWAR kernels pass 16".
- **Addendum (h8 = h5 with the helper declared `m2, m1, x0, a1, w1, s, d, a0, w0, stride, x1`):** case 0 keeps the target's opcode order; the
  trio stays level 2 (33: cases 1/2 temps still present) so the real colours cannot confirm the model yet, and a NEW effect: with the constant
  locals declared FIRST, m2's web is dead (`@61` 0/0 = constant-propagated into its uses, `lis/subi` per case, size 0x320) while declared LAST
  (h5) both masks stay webs (29/30). The tree's `#pragma opt_propagation off` was there for this; whether the vendor's order or a pragma kept
  the masks is open — read m1/m2's AST in the next pass before fixing the order. Harness kept small (`ra_h5`, `ra_h4c`, specs, listings);
  ~/.cache/cri_swar15 deleted.

### CRI pass 62, part 2 (continuation of "CRI pass 62" above — another agent's SWAR pass 16 section landed between; adx_dcd5 Ste4AsSte 25w / Ste4AsMono 143w read in the model, three requirements named, none spelled in C; nothing applied, nothing flipped; 2026-09-12)
- **adx_dcd5 Ste4AsSte 25w / Ste4AsMono 143w (read only, nothing applied, tree unchanged incl. the Ste pin):** the target is reproduced by the
  model (chaitin.py + graph edits, IDENTICAL/exact on both dumps) from THREE independent facts, none of which has a pure-C spelling yet:
  1. **Two more never-removed nodes adjacent to every loop value** (pass 47/52's "(#r6 + #r8 == 2)"): coalesced parameter copies of histl/histr
     (or c1/c2). Mechanism re-read on the dumps: a parameter copy `mr rV, rP` (B1) survives backend-05 copy propagation only when rV is
     multi-def (src/outl/outr: their `addi` steps) or is the source of a copy the propagation cannot fold (nfrm: the return `mr r3, r33`; the
     `?:` two-def web of pass 52); a surviving copy is coalesced at the RA = the ghost. Negatives this pass (a7 base, 115w, ghost list
     unchanged `r3 r4 r5 r7 r1`): `histl = histr;` before the right-channel stores (later web = compiler copy, folded to r8), `histl = histl;`,
     `c1 = c2;` dead at the end, `c1 = (Sint16)c1` / `(Sint32)c1` at the top (new @temp `extsh r22,r9`, 75-106w), `c1 += 0`, `const`/`register`
     parameters, `#pragma opt_propagation/opt_lifetimes/opt_common_subs/opt_dead_assignments/opt_loop_invariants/opt_strength_reduction off`
     around the function (accepted without warning, no effect at -O4,p). "c1/c2 live in physical r9/r10 across the loop" (phys adjacency copied
     from @64/@63) is NOT the target (11/16, smul spills to r21): the target's `extsh r9,r9` at the top is the @temp coloured onto the freed
     parameter register, as in the matching Mono4 (`extsh r7,r7`).
  2. **sadd in the final scan, coloured before smul (r0/r11/r12):** sadd (r42, deg 76) lacks exactly the two never-removed neighbours smul (78)
     and scl (80) have: phys r1 and the argument-base ghost r72 (`lwz r72,r1,0` -> `lha r42,r72,0(sadd)` is its LAST use in ours' pre-RA
     schedule, so r72 dies at sadd's load). Adding sadd-r72 (or sadd-r1) alone in the model gives sadd r0, smul r11, scl r12 (the vids r42 > r41 >
     r40 then order the final scan); adding r0 as well flips sadd/smul. So in the target's pre-RA schedule the sadd load is not the last
     argument-base use: the three stack-argument loads are emitted in B1 in declaration order and the pre-RA scheduler orders them by height
     (scl > smul > sadd here); a shape that gives sadd's load a longer dependent chain, or another argument-base use after it, is the lever
     (not found; the ABI offsets 0x48/0x4e/0x52 fix the declaration order). A ghost on r9 or r10 (`+ghost:9:phys=9`) does the same thing.
  3. **qtbl as the own local r50 (coloured after sc_r -> r22):** `qtbl = AdxQtbl` is `lis r75; addi r76; mr r50,r76` and backend-02 copy propagation
     folds the single-def `mr` (the loads use r76, which is coloured FIRST in its level -> r31 once 1./2. hold, the last 11 words of the model).
     Own-local `mr` copies survive only when the destination is multi-def (adx_baif's `mr r43,r65; rlwimi r43` chains; Ste's `mr r58,r48` rr2 = t).
     Negatives (115w): `qtbl = AdxQtbl` repeated at the end of the frame loop (128w, +8), `(const Sint32 *)(Uint32)AdxQtbl`, `const *const` initialiser,
     `&AdxQtbl[nblk & 0]`.
  With 1.+2. on the tree's pinned graph (`+ghost:9`) or 1.(ghosts 6,8)+2. on a7, chaitin scores 5/16 with every miss explained by r76's rank
  (3.); with the pin alone 13/16. Mono (143w): ours L4 = sadd r0, smul r11, scl r12; the target has scl r11, smul r19 (coloured after nblk r18)
  and l2 in r12 — smul must lose most of its 92 neighbours before nblk/t are removed, i.e. a different web structure (not read further).
- Harness deleted; /tmp/kit.* outputs of this pass removed. Tree: src/lib/adx_baif.c + objects.py only. adx_dcd5 2/4 unchanged.

### CRI SWAR kernels pass 17 (mpv_mc 8x8 H2 494 -> 34w APPLIED: cases 0/1 byte-identical, all four cases unrolled with the target's colours, size 0x3ec; the in-place pack = an OR of a shifted value and a complement mask of the destination; V2 73w, mpv_mcy H2 197w / V2 225w unchanged; nothing flipped; 2026-09-12)
Harness ~/.cache/cri_swar17/ (cri_swar16's scripts with the path changed + `mkrow.py NAME BASE.spec K < row`, `r1.sh`/`c2v.sh` = probe + per-case
order diff, `sw.py`/`subsets.py`/`cmp2.sh` = sched.py pre-RA what-ifs with `inplace=V:T` (delete the K6 `mr T, V`, rename T -> V) / `del=i`,
`troles.py` = the target loop in vreg roles, `post.py` = the post-RA model with physical-register edits; specs h9a/h9b/h10/h11a-b/h12a-b).
- **The in-place insert without a node and without a copy in the DAG: `a0 = w0 << 24; a0 = (w1 >> 8) | (a0 & 0xFF000000);`** (h12a). The
  or->rlwimi peephole (pass 01) fuses the EARLIER-defined rlwinm (`w1 >> 8`) and takes the other as the base; a base rlwinm with rotate 0 whose
  mask is the complement of the insert range is replaced by its SOURCE register (`mr @a0, @a0` -> deleted by pass 02), so the rlwimi writes the
  variable's own web: `slwi a0, w0, 24; rlwimi a0, w1, 24, 8, 31` with a same-vreg WAR edge and only the fused srwi left as a dead def
  (+1 pcode per pack). When the mask is a no-op on the value (a0 = w0 << 24 has zero low bits) pass 01 first turns it into a copy. For a loaded
  value (`a0 = W(s, 0)`, case 1) the mask is not a no-op: `a0 = (w1 & 0xFFFF0000) | (a0 & 0xFFFF)` leaves BOTH rlwinm as dead defs (+2) but
  still no copy (h9b: `rlwimi a0, w1, 0, 0, 15` on the lwz, then `a0 = __rlwinm(a0, 16, 0, 31)` = `rotlwi a0` in place, no K6 copy).
- **The peephole's fused-operand rule (h6a/h12b/h12a dumps):** if one OR operand is the destination register itself (`a0 |= e`, `a0 = e | a0`),
  THAT operand is fused (its rlwinm def must not have destroyed its source, else the `or` stays: `w0 <<= 16; w0 |= w1 >> 16`); otherwise the
  earlier-defined rlwinm is fused and the later one is the base (`mr D, base` unless the base is a redundant/no-op mask of a register).
- **K6 intrinsic on the variable (`w0 = w1 >> 24; w0 = __rlwimi(w0, a0, 8, 0, 23)`, `w1 <<= 8; w1 = __rlwimi(w1, a1, 24, 24, 31)`, `a1 =
  H(s, 8); a1 = __rlwimi(a1, w1, 16, 0, 15)`): 3 pcodes (rlwinm, K6 mr, rlwimi), the reads of the variable are propagated to the temp and
  the K6 `mr t, @V` coalesces iff the scheduler leaves no overlap between t and the web's NEXT def** (case 1: all three coalesce, h10 case 1 =
  58/58 identical; case 2's `a0 = w0 << 24; a0 = __rlwimi(a0, ..)` does not: `and @a0 = x0 & m1` is scheduled before `and w0 &= t` (t frees
  nothing, a0' frees its srwi) -> r62 node r0, x1 pushed to r3).
- **Case 2 (h12a): the four in-place mask packs = 35 pcodes -> unrolled, colours = the target's (r4 w0, r5 a0, r8 w1, r9 a1, r10 x0, r0 x1,
  r11/r12 m1/m2, r7 s, r3 stride, r6 d), residue = order (28 lines): sched.py (9/9 blocks identical pre- and post-RA on these dumps) shows the
  target's case-2 order IS the pre-RA schedule of the in-place DAG with NO dead defs (`subsets.py`/`cmp2.sh del=..`: 0 lines); our four fused
  `w1 >> 8`/`w1 >> 16`/`x1 >> 8`/`x1 >> 16` dead defs carry a WAR edge to the in-place `slwi w1/x1` (height 18) and make `lwz w1` free 3
  successors -> loaded before `lwz w0`. Cases 0/1/3 differ: their target order = ours' post-RA reschedule after the K6/or-pack copies are
  deleted (post.py 0/1 diff lines). Open: a C form whose fused shift is not a separate dead pcode (or whose dead def has npreds > 1).
- Words: h12a = H2 34w (cases 0/1 identical, case 2 order 28 lines, case 3 order 4 lines: `rlwimi w0, a0` placed after `lwz a1` in ours),
  size 0x3ec exact; tree untouched so far.

### CRI pass 63 (sweep of the remaining CRI residues with the pass-62 retype lever and the SWAR-16 `@N`-web coalescing lever: mwsfdcre CalcWorkSfd 4w / CreateSfd 115w, adx_dcd5 Ste4AsSte 25w, sfd_tst 79w, mpv_umc OneReadMb 48w, sfd_mps 5w, sfh_main 6w, adx_tsvr 2w — neither lever addresses any of them; every residue's mechanism re-read one step further; nothing applied, nothing flipped; 2026-09-12)
Harness ~/.cache/cri63/ (`mk.py SRC OUT 'OLD=>NEW'..` exact-string edits, `t.sh UNIT FUNC VARIANT.c` = one word line via variant.sh (`SHOW=n` adds the `*` rows), ra.py / scheddump dumps; deleted at the end). Tree untouched (no source edit, objects.py untouched, no ninja).
- **Where the retype lever applies, read exactly (mwsfdcre `mwPlyCalcWorkSfd`, 30 probes):** MWCC retypes a `long` read to `EINDIRECT int` ONLY under a relational/equality operator against an `int` constant. Arithmetic with an `int` operand (`int sibsiz + long size`, both orders), an `int` return type (`int f()` returning a long expression), `(int)` casts, an inlined helper with `int` parameters (`ETYPCON int (EINDIRECT long)` at parse), `switch (size)` (`EINDIRECT long`, the empty switch is deleted at the frontend) all leave the read `long` and the single-use local is pulled (13w). Every retyped compare is code: `if (size == 0) {}`, `size == 0;`, `size2 = (size == 0)` (dead), `(size == 0) & 0`, `if ((size == 0) && 0)`, `while (size == 0) break;` are all deleted BEFORE the substitution count (13w); `(size == 0) ? size : size` keeps the compare as `addic. r3,r3,0x4800` (1w, size equal: the backend removes the identical-arm branch but merges the compare into the addi, = pass 60's `>` form); `if (size == 0) return 0;` shows the kept-variable tail (`add r3, r29, r3`, epilogue not hoisted) at +12 bytes. So the lever keeps a variable only where the target HAS the compare (adx_baif's ckid); it is not a codeless second read. Also read: the tree's `size += sibsiz; return size` is not add-propagated because the web is multi-def; `size = sibsiz + size` / `return sibsiz + size` with a pulled `size` are reassociated by the codegen (`add; add; addi`), and a kept `size` with a single-def `addi size,t,K` is add-propagated into the final add — the target's `addi r3,r3,0x4800; add r3,r29,r3` therefore needs `size` multi-def at that point (as `+=` gives) AND no `mr` in the block. Left 4w. CreateSfd 115w not probed (an arm compare is never deleted; passes 6/7/51/60 cover the arm space).
- **adx_dcd5 `ADX_DecodeSte4AsSte` 25w — the `@N`-web lever does not create the missing ghosts.** Whole body as `static inline adxdcd_ste(11 params)` + `#pragma inline_max_size(2000)` (needed: `-inline auto` leaves a `bl`): 189w without the pin, 171w with it, 191w with `#pragma opt_lifetimes off` around the caller, size -4. Ghost list of the helper form: `r32->r3 src, r33->r4 nfrm, r34->r5 outl, r36->r7 outr` (the own copies) PLUS `@78->r3 src, @79->r5 outl, @80->r7 outr, @59->r4 nfrm` (the helper's parameter webs: only the multi-def ones survive, and each becomes a SECOND ghost on the same physical register) plus @76->r25/@66; the histl/histr/c1/c2 helper webs are propagated away like the own copies (`0/0`). So an inlined helper doubles the existing ghosts instead of adding the two the target needs (pass 62 part 2 requirement 1). Pre-RA B1 read (scheddump): the three stack-argument loads (`lwz scl; lha smul; lha sadd` from r72 = the argument base) are emitted at B1's top in parameter order and tie on every criterion (height 2, no successor but the terminator, same class) -> input order -> sadd's load is r72's last use by construction (requirement 2); no C-visible pcode reads the argument base after it. (Mistake this pass: `*histl++ = l1; *histl = l2;` re-probed — pass 54's negative, peephole-forward folds the addi at backend-01 before copy propagation.) Left 25w/143w.
- **sfd_tst `SFTST_Calc` 79w:** `diff` (Sint64) needs a live second definition (pass 16) — `diff += 0`, `|= 0`, `*= 1`, `&= -1`, `<<= 0` after the abs diamond are all folded by the frontend (79w); `diff = (diff < 0) ? diff : diff` 128w (+0x30). The retype lever cannot apply: `diff < 0` is `ELESS int (EASS long long, EINTCONST long long 0)` — a `long long` operand is not retyped (only same-size `long` vs `int` is). Left 79w.
- **mpv_umc `mpvumc_OneReadMb` 48w (the c33 `rlwinm chx` vs `rlwinm yhx` tie of pass 49b):** statement permutations (`cvy` before `cvx`, `vy` before `vx`, fn_y before yhx, `(src + cpitch) + chx`, `tbl_y[vy & 1][yhx]`) 48w; `yhx = vx & 1` / `chx = cvx & 1` without the `(Uint32)` cast 52/53w; cvx/cvy first 54w. Dump fact: the table-index masks `vx & 1` / `cvx & 1` (r76/r93, dead after peephole-forward folds them into `clrlslwi ..,31,2`) are CSE'd INTO yhx's/chx's `clrlwi` (r46/r45) and vanish, while the `vy & 1` / `cvy & 1` leftovers (r73/r90) survive as dead leaves to the RA — the pass-60 dead-def lever gives yhx no second reader here because no target instruction shifts yhx (its only readers are the `and` and the `add`). No compare exists in the function, so the retype lever has nothing to keep. Left 48w.
- **sfh_main `SFH_AnlyElemSmpHz` 6w (M4 stwbrx):** the post-RA fold fires only when all four rotate terms read ONE register: `SWAP32((Uint16)SFH_ELEM_SMPHZ(elem))` (the pass-62 CSE breaker) leaves the chain in place (5w: a `clrlwi r4` mask node, the base term reads the unmasked load), `*(volatile Uint32 *)` too (20w, four loads). Casts and views that do not add a mask (`(Uint32)`, `(Sint32)`, `(void *)`, `[0]`, `&..[0]`, `(Uint8 *)elem + 0x1c`, `+ 0`) are all frontend-CSE'd into the one @temp and fold (6w). Ours after regalloc = the target's chain instruction for instruction (`lwz r4; rlwinm r0,r4,8,8,15; rlwimi x3; stw r0,0(r5)`, word r4 vs r6) and the post-RA peephole folds it; what kept the vendor's identical chain unfolded is still the M4 question (the r6 colour of pass 11 is the only visible difference). Left 6w.
- **sfd_mps `sfmps_DecodeOneUnit` 5w / adx_tsvr `adxt_nlp_trap_entry` 2w:** no Sint32-vs-int compare whose variable ours substitutes exists in either (sfd_mps: delim/len/total/flags are multi-use, cres address-taken; adx_tsvr: n1 multi-use, n2 two-def). adx_tsvr probes: `n2 = (n1 == 0) ? -1 : ADX_ScanInfoCode(..)` 2w (unchanged), `n2 = -1; if (n1 != 0) n2 = ..` 13w, arms swapped 9w, `ofst2v = ofst2` before `ofst1 += ofst` 2w. Left 5w / 2w.
- Catalogue note (MWCC row "single-use local substituted"): add the scope — the retype needs a relational/equality operator with an `int` constant on a `long` (Sint32) object; `long long`, arithmetic, casts, returns, arguments and `switch` never retype, and the compare is always emitted (identical-arm `?:` = record form on the def). Row "pack written straight into a variable" (helper `@N` webs): a helper's parameter web survives to the RA only if multi-def, exactly like an own parameter copy, and then coalesces onto the SAME physical register as the own copy (a duplicate ghost, not a new one).
- **APPLIED to src/lib/mpv_mc.c (H2 494 -> 34w, size 0x3ec exact, locked ninja + bytecmp; 4p/1p/Init still identical; NOT flipped: V2 73w):**
  `static inline mpvmc08_OneRefH2Body` (helper declaration order `m2, m1, x0, a1, w1, s, d, a0, w0, stride, x1, i`, one-row `for (i < 8)` per
  case), case 0 = the tree's row with `a1 = __rlwimi(a1, w1, 8, 0, 23)`; case 1 = `w0 = w1 >> 24; w0 = __rlwimi(w0, a0, 8, 0, 23); a0 = (w1 &
  0xFFFF0000) | (a0 & 0xFFFF); a0 = __rlwinm(a0, 16, 0, 31); a1 = __rlwimi(a1, w1, 16, 0, 15); .. w1 <<= 8; w1 = __rlwimi(w1, a1, 24, 24, 31)`
  (roles as the target's colours: the `lwz 0` word is a0, the pack is w0); case 2 = four `V = shift; V = (fused) | (V & mask)` packs; case 3 =
  `w0 = __lwbrx(s, 0); w0 = (a0 >> 8) | (w0 & 0xFF000000)`. Three tagged pragmas: `opt_propagation off` (masks), `inline_max_size(100000)` +
  `inline_max_total_size(100000)` (the helper is `bl`'d without them: h13a; `-inline all`, `-inline auto,level=8`, `-inline on`, `-inline
  deferred,all` on the production command do NOT inline it either — no per-unit flag reproduces it; a smaller/per-row helper would give
  per-call-site webs = pass 15's ghost budget problem, untested), `opt_lifetimes off` around the caller. Residue 34w = case 2 (28 lines, order)
  + case 3 (4 lines: `rlwimi w0, a0` after `lwz a1` in ours, before it in the target) — both = the fused-shift dead defs in the pre-RA DAG (the
  clean DAG reproduces every case's target order in the model, cases 0/1 also through ours' post-RA reschedule).
- **Masks (item 3):** the H2 target materialises m1/m2 ONCE (2 `lis` at the top, function-level webs r11/r12); the V2 8x8 and 16x16 H2/V2
  targets materialise them PER CASE (8 `lis` = propagation on). Without `opt_propagation off` the H2 masks are propagated into every case
  (lis/addi per use, bodies 37-45 pcodes, no unroll, 412w); masks as helper PARAMETERS are propagated too (h13b, 8 lis); a conditional second
  def keeps only that mask (h13d). Declaration order does not matter (pass 16's "m2 dead" was `i`'s web @61 after ctr conversion). So the
  vendor's H2 had the masks non-propagated by a pragma or a construct not found; the pragma is tagged.
- **8x8 V2 case 1 (73w, item 5, not closed):** the kept `lbz r31 (a2); mr r28, r31; rlwimi r28, r30, 8, 0, 23` needs a2 to stay an own-local
  web AND to be coloured r31 (adjacent to a1' r28, w2 r29, a1 r30 = a2 live past the copy). `a1 = __rlwimi(a2, a1, 8, 0, 23)` (v1a, 82w)
  substitutes the lbz into the intrinsic (K6 on a load coalesces: `lbz r28; rlwimi r28`); pass 15's `s1 += stride` between load and use keeps
  the web but a2 takes r28. The V2 target is a plain function (per-case masks), not the helper form. Left 73w.
- **16x16 H2 197w / V2 225w (item 6, read only):** the target KEEPS its pack copies (`mr r22, r7; rlwimi r22, r21` = own-local destinations,
  lifetimes on) and uses r20-r31 (`stmw r20`) where ours uses r21-r31: one more simultaneously-live callee-saved value in the target
  (`lbz r20, 0x10(r4)` = the byte a3 in a saved register). Colour residue only; the helper form is NOT the vendor's here (copies kept).
- Harness: ~/.cache/cri_swar16 DELETED; ~/.cache/cri_swar17 kept SMALL (scripts, specs, bodies, t_*.s; dumps deleted).
  Tree: src/lib/mpv_mc.c H2 (above). objects.py untouched (mpv_mc stays False); no `ninja -k 0`, no 111 (nothing flipped).

### CRI pass 64 (sfd_tst Matching 10 -> 11/11: SFTST_Calc 79 -> 0w pure C, no pins; the colouring residue was `tol` being the inlined Conv's `@123` temp instead of an own local, plus the hist loop's `i` as a helper web and SumHist's i/sum declaration order; 2026-09-12)
Harness ~/.cache/cri64 (wi.py = chaitin what-if with `mv=VID:AFTER` scan-order moves / `addn=VID:N` never-removed ghosts / `noedge`, scored
against the 16 target colours of the three regions; search.py = single-web sweep; ra_base/ra_B1 dumps; deleted at the end).
- **The what-if that reproduces all three regions at once (16/16 in chaitin.py, ours IDENTICAL to the compiler apart from 4 cost lines):**
  give `tol` (@123, vids 93 lo / 94 hi) an OWN-LOCAL vid below `ave` (44/45): `mv=93:43 mv=94:93`. Nothing else. Mechanism: in the L2 scan
  (ascending vid) ours removes tol (93/94) AFTER visiting ave.hi (45, degree 30 = 28 + the two tol edges), so ave.hi survives to L3 and is
  coloured at index 8 with a NEW r23; MulDiv (L2) then takes r23 instead of the free r24, diff.lo r23, adiff.hi r25, tol.hi r22 -> every
  region shifts. With tol below ave, ave.hi is visited after tol in L2 and drops to 28 -> L2; colouring: MulDiv r24 (lowest free handed-out),
  diff.lo r25, merged adiff/diff.hi r23, adiff.lo r22, sprintf group r22..r30 from out.hi, then ave.hi r22, tol.hi r23, tol.lo r21, and mt.hi
  (L1, last) r21. Negatives in the model: a `diff` own-local vid (2/16), MulDiv +9 (4/16), ave.hi -2 edges (13/16 but not realisable), a
  @temp `ave` between @119 and @123 (16/16 in the model, but the helper form `ave = sftst_CalcAve(tst)` coalesces the divw into ave.lo: -4 bytes,
  115w).
- **Spelling: `tol = mt->unit * tst->tolerance.cnt / tst->tolerance.unit;` (the Conv expression written inline) 79 -> 38w, size equal.** Why:
  `tol = sftst_Conv(&tst->tolerance, mt->unit)` = an own local assigned a plain copy of the inlined helper's return temp; the frontend replaces
  the single-def local by the @ret temp (frontend-01: `EOBJREF [@123]`, no `tol`), and a @temp's vid is ABOVE every own local. An own local
  assigned an expression (`ave = SumHist(tst) / range`, `est = ..`) keeps its name and its low vid. Catalogue row 1 addendum: "own locals in
  reverse declaration order" holds only for locals that survive the frontend; a local that is a pure copy of an inline's result becomes that
  @temp and ranks with the temps.
- **38 -> 16w: the `hist[i] -= (Sint32)step` loop as `static void sftst_SubHist(SFTST tst, Sint32 sub)`.** The 38 were one node: Calc's own `i`
  (r40, 30 total = L2) is coloured before every L1 temp of the step block and takes r3; the target's counter is r6 = coloured AFTER the block's
  temps (r3/r4/r5) and BEFORE q (r8 lo / r5 hi). Model: `i` needs a vid above q's (52/53) and below the backend temps AND <= 28 neighbours —
  an inlined helper's local (@106 in the dump: 26/30, L1, coloured at index 322 -> r6; 12/14 with the model's `noedge` + `mv=40:104`). The
  helper form gives the identical schedule (the `li i,0` still hoisted to the block top).
- **16 -> 0w: `Sint32 i; Sint32 sum;` in sftst_SumHist (i declared first).** The second inline's sum/i (@111/@112 in the C1 dump, both L1 with
  23 neighbours) are coloured in descending vid: ours i (@111, vid 101) first -> r4, sum -> r5; target sum r4, i r5. Inlined helper locals are
  numbered @N in REVERSE declaration order at the inlining point (frontend-00: `@99` = ret temp, `L@100`, `@101` = i, `@102` = sum for
  `Sint32 sum; Sint32 i;`), so the vid (reverse of @) ascends in declaration order — the catalogue row "helper-local vids ascend in declaration
  order" holds; declaring `i` first gives sum the higher vid. The first inline's pair is unaffected (r0 is blocked for its `i` either way, model
  checked both swaps). Calc's now-unused `Sint32 i` removed (IDENTICAL with and without it).
- Tree: src/lib/sfd_tst.c (three C edits + comments, no pins, no pragmas); objects.py `# CRI pass 64` block `"lib/sfd_tst.c": True`; locked
  `ninja -k 0`; `dtk shasum -c` 111 OK. Harness ~/.cache/cri64 deleted.
- Catalogue additions (MWCC rows 1/13): (a) a local assigned a PLAIN copy of an inlined helper's return value (`tol = sftst_Conv(..)`) is
  replaced by the helper's @ret temp and ranks with the @temps, above every own local — write the expression out (or make the assignment
  more than a copy) to keep it an own local; (b) a loop counter that must be coloured after a block's temporaries but before other own locals
  = an inlined helper's local (vid between the own locals and the backend temps, level 1 when its degree allows).

### CRI SWAR kernels pass 18 (mpv_mc Matching 3 -> 5/5 FLIPPED, 111 OK: 8x8 H2 34 -> 0w (mask `lis` order = initialiser statement order; cases 2/3 inserts as asm-emitted `rlwimi`, tagged: no C spelling gives a leftover-free DAG and the scheduler model shows every leftover node moving the order), 8x8 V2 73 -> 0w pure C (range-split webs are numbered by each variable's FIRST definition = dead initialisers order the colouring; the third load kept by the pointer step after it and the second pack written INTO it); mpv_mcy 16x16 H2 197w / V2 225w read only; 2026-09-12)
Harness ~/.cache/cri_swar18/ (cri_swar17's scripts + `mk.py NAME [H2|FUNC] [UNIT]` = splice bodies/NAME.c over the whole H2 pragma region or one
function, `mkc.py NAME CASE [BASE] < row` = one case's loop body replaced, `c2.sh NAME [CASE] [BASE] < row` = words + pass 03/04 counts + rows.py order diff,
`dagx.py OUTDIR BLOCK ROLES [k6=D:R|k6x=D:R|del=I|coal=I|ren=A:B ..] --both --rowlen=N --mr` = pre-RA schedule of a block after DAG transforms (K6 copy
inserted / dead def deleted / copy coalesced) diffed role-blind against `troles.py` output, `cmp.py` = cmp2.sh with dead-def lines dropped; NOTES.md).
- **H2 8x8 34 -> 30w: the two mask `lis` temps.** Target `lis r5, 0xfeff (m1); lis r4, 0x101 (m2); subi r11 (m1); addi r12 (m2)`: the lis temps are
  backend temps coloured in creation order = the INITIALISER STATEMENT order (later-created = higher vid = coloured first = r4), while the mask webs'
  colours (m2 r12, m1 r11) follow the helper-local DECLARATION order (first declared = lowest vid = coloured last). The tree had `Uint32 m2 = ..; Uint32
  m1 = ..;` (both orders tied to the declaration); `Uint32 m2; Uint32 m1;` + `m1 = 0xFEFEFEFE; m2 = 0x01010101;` after the locals gives both.
- **H2 8x8 30 -> 0w: cases 2 and 3 need a DAG with NO leftover pcode.** dagx.py over all 4^4 combinations of {or-pack (dead fused shift), `V =
  __rlwimi(V, ..)` (K6 `mr t, V` node), `V = __rlwimi(V << k, ..)` (K6 `mr t, t1`), clean} per pack: ONLY the fully clean DAG reproduces the target's
  case-2 order (0 lines; the best mixed form is 12), and case 3 likewise (0 lines only with both packs clean; `w1 = (a1 >> 8) | (a0 << 24)` = dead
  srwi + coalesced `mr` = 20/24 lines). The target's case-2 order is stable under the post-RA scheduler (post-RA model on the target order = the target
  order), so a dirtied block in the vendor's build gives the same bytes. Every C spelling of this compiler leaves a pcode: the or->rlwimi peephole
  keeps the fused rlwinm as a dead def (deleted at RA only), the intrinsic always emits the K6 copy (also on an rvalue first argument), a user copy
  into a helper local is an `mr` node until the RA. **Lever (tagged COMPILER-DIFF): `V = base_shift; asm { rlwimi V, src, sh, mb, me }` on `register`
  helper locals** (`asm` operands must be `register`; the asm pcode is an ordinary RLWIMI flags 0x88000000 to the scheduler; the `register`
  qualifier did not change the helper locals' colours). Case 2: 35 -> 31 pcodes (still unrolled), case 3: 28 -> 26. Case 0/1 untouched (identical
  already through the post-RA reschedule). The vendor's "TuneC" evidently had these inserts as inline asm. Tree: src/lib/mpv_mc.c H2 helper +
  comment; locked ninja + bytecmp: H2 0w, mpv_mc 4/5 identical (V2 73w).
- **8x8 V2 73 -> 56w APPLIED (`Uint32 x0 = 0, x1 = 0;`): the frontend's range-split webs (`@N`, lifetimes on) are NUMBERED BY THE FIRST DEFINITION
  OF EACH VARIABLE in the function (all of a variable's webs consecutive, cases in reverse AST order: case 3 lowest @), not by declaration order
  (three declaration permutations: same numbering), and the RA colours them in ascending @.** The tree's order was w0, a0, w1, a1, x0, x1 (case 0's
  statement order); v2perm.py (chaitin.py with the six variable groups permuted, 6! x 16 pack/load flips) finds cases 1 and 2 fully right (20/20
  webs) only with x0 and x1 FIRST: x0 takes r8 before the a0 load, so x1 finds every volatile register blocked and takes r31 (the function's first
  callee-saved hand-out), a0' r9 / a0 r8 follow. A dead `x0 = 0; x1 = 0;` before the switch or the initialised declaration moves the first
  definitions (deleted, no code; size unchanged): case 2 identical, case 1 = the kept `mr r28, r31` (a2) + the s0/s1 add positions, case 3 colours.
- **Case 1's a2 (not closed):** a2 is kept as a web by `a0 = W(s1, 0); a2 = s1[8]; s1 += stride;` (q6/q10: `lbz a2; mr @a1', a2; rlwimi`), but the RA
  gives a2 the pack web's colour r28 (a2 is NOT adjacent to the copy's destination, and r28 is the lowest free handed-out callee-saved) and
  deletes the copy; movevid.py (a2 at every vid slot of q6's graph) never yields r31 (r28/r30/r11/r8/r3). The target's a2 (r31) therefore either
  interferes with a1' (a2 live past the copy) or is coloured after w1'(r29)/a1(r30) and before a1'(r28) — i.e. its web sits INSIDE the a1 group's
  vid range, which no first-def order of a separate variable gives. Negatives: `Uint8 b0, b1` (substituted like Uint32), `(a2 | a2)` (folded).
  Case 3's target reads as w2 (r11) and a2 (r28) coloured AFTER the w0/a0 webs (webs, not temps) with w1 the first callee-saved (r31).
- **8x8 V2 56 -> 24 -> 11w APPLIED (size exact now).** (a) The third load of each row (`w2 = s0[8]` / `a2 = s1[8]`, the half in case 2, the
  word in case 3) is KEPT as a variable in the target (case 1 `lbz r31; mr r28, r31; rlwimi r28, r30`; case 3 `lwz r28; srwi r28, r28, 8` in
  place and `lwz r11 (w2); srwi r29, r11, 8`: loads coloured AFTER the w0/a0 webs = webs, not backend temps). Pure C: the pointer step right
  after the last load through it (`w0 = W(s0, 0); w2 = s0[8]; s0 += stride; a0 = W(s1, 0); a2 = s1[8]; s1 += stride;`) blocks the frontend's
  single-use substitution (the load cannot move past the pointer update); the `add r5/r6` land where the scheduler puts them (late, as in the
  target). With x0/x1 dead-initialised: 24w, the `mr r28, r31` appears, size 0x330 exact. (b) The first-def order `x0, x1, w0, a0, w2, a2, w1,
  a1` (dead initialisers `Uint32 x0 = 0, x1 = 0, w0 = 0, a0 = 0, w2 = 0, a2 = 0;`) -> 11w: cases 0/1/2 identical; case 3 = the callee-saved
  rotation w1/a1/w1'/a2 (target r31/r30/r29/r28, ours r29/r28/r30/r31). Range-split detail: with a variable kept in cases 1-3, ONE case's web
  stays the own-local node (case 1 here, lowest vids) and the others are `@N` webs (case 3 lowest @). exh6.py (720 first-def orders of the six
  row variables after x0, x1, 1 s each) running for the case-3 rotation at the time of writing (result below).
- **16x16 H2 197w / V2 225w (read, nothing applied):** the dead-init lever gives at most 197 -> 194 (climb16.py over the 13 variables); the
  case-0 byte (`lbz r20`) is a3's own-local web (`a3 = s[16]; a3 = (w3 << 8) | a3`), coloured last, and takes r20 in the target because every
  volatile register is blocked at that point: the target's level-2 own locals (volatile r9-r12) are i, w1, a1, w2 where ours are i, w2, w3, a2,
  x2, x3 (+x3 r28) — a live-range (degree >= 29) difference of the case-0 body, not a vid order. `w4 = s[16]` / a separate `b` (219w, size -8:
  substituted, the pack copies coalesce), x's interleaved with the packs 196w, per-pair statement blocks 231w, reversed pack order 299w.
  Needs its own pass: role-map the four cases' webs against the target and search the case-0 statement order in chaitin.py.
- **8x8 V2 11 -> 0w, mpv_mc IDENTICAL and FLIPPED (objects.py `# CRI SWAR pass 18` block, locked `ninja -k 0`, `dtk shasum -c` = 111 OK).** The
  case-3 rotation (and the 720-order exhaustive search: best 11w with `w0, a0, w2, a2, w1, a1`) was the wrong lever: the target's loads of
  w1/a1 are coloured BEFORE the packs that replace them (w1 r31, a1 r30, then w1' r29, a1' r28), and within one variable's group the later
  definition always gets the LOWER @ (reverse AST order: pack before load, case 3 before case 1), so the second pack cannot be the same
  variable as its load. **Form: the second pack is written INTO the third variable: `w2 = (w1 << 8) | w2; a2 = (a1 << 8) | a2; x1 = w2 ^ a2;
  d[1] = AVG2(w2, a2, x1)`** (case 2 `<< 16`, case 3 `w2 = (w1 << 24) | (w2 >> 8)`), w1/a1 hold the loads only. With the pointer steps after
  the third loads and `Uint32 x0 = 0, x1 = 0;` (both still required: 31w without the dead initialisers) = 0w, size 0x330. The or with the
  destination operand a plain LOAD (`w2 = (w1 << 8) | w2`) gives `rlwimi w2', w1` on the load's web with the copy coalesced (case 2: `lhz r28;
  rlwimi r28`), on a kept own-local (case 1's a2, the lowest vids) the copy stays (`lbz r31; mr r28, r31`) because the own local is coloured
  last and r31 is its only free callee-saved (r28 a1', r29 w2', r30 a1 blocked), and in case 3 the `(w2 >> 8)` base is `srwi w2', w2` /
  `srwi a2', a2, 8` written on the same register when the load web is coloured after the pack web with r28 the lowest free. Separate pack
  variables p0/q0/p1/q1 (q17/q18: 93/64w, packs coloured last = callee-saved) and the first pack into w1 are wrong.
- **Catalogue rows (MWCC table) from this pass:** (1) "range-split webs (`@N`, lifetimes on) coloured in a wrong order / a variable's web takes
  the wrong callee-saved" -> the webs are numbered by the FIRST DEFINITION of each variable in the function (all of a variable's webs
  consecutive; within a variable later definitions and later cases get the lower @) and coloured in ascending @ before the own locals ->
  dead initialisers in the declaration (`Uint32 x0 = 0, x1 = 0;`, deleted, no code) put a variable's group first; a load and the pack that
  replaces it cannot be reordered within a variable (write the pack into another variable). (2) "a pre-RA order that only the leftover-free
  DAG reproduces (dagx.py: every or-pack dead def / K6 copy / own-local copy moves it)" -> `asm { rlwimi V, src, sh, mb, me }` on `register`
  locals, tagged (the asm pcode is an ordinary RLWIMI to the scheduler; the vendor's TuneC had inline asm there). (3) "a single-use load
  substituted in ours, kept as a variable in the target (`lbz r31; mr r28, r31`, or a load coloured after the webs it feeds)" -> place the
  pointer step (`s += stride`) right after the load; the scheduler still sinks the `add` to the target's position.
- Harness: ~/.cache/cri_swar17 DELETED; ~/.cache/cri_swar18 kept small (scripts: mk.py/p.sh/mkc.py/c2.sh/dagx.py/cmp.py/
  v2perm.py/movevid.py/climb16.py/climbv2.py/exh6.py/v2.sh/h16.sh, bodies/, NOTES.md; dumps deleted). Tree: src/lib/mpv_mc.c (H2 helper +
  V2), config/G4BE08/objects.py. mpv_mcy untouched (197w/225w).

### CRI pass 65 (sfd_mps Matching 25 -> 26/26 FLIPPED, 111 OK: sfmps_DecodeOneUnit 5 -> 0w pure C, M1 pin removed — the two missing ghosts are the PES callback pair's call-result copies as inlined-helper locals; adx_tsvr 2w / sfh_main 6w read in the model (both need an INVISIBLE r0/r4 node live in the join/swap block); 2026-09-12)
Harness ~/.cache/cri65 (wi.py = chaitin what-if: `mv=VID:AFTER`, `edge=A:B` (B may be physical), `ghost=VID:PHYS`, `ghostlike=VID:PHYS`, `--target v=reg,..`; ra_* dumps; deleted at the end).
- **sfd_mps `sfmps_DecodeOneUnit` 5 -> 0w (cnt r21), IDENTICAL, flipped.** Pass 55's requirement (+2 never-removed ghosts adjacent to `ret`, on the pinless `?:`-n base) is met by
  `static void sfmps_SetPesFns(SFD sfd, MPS mps) { void *fn; void *obj; obj = (void *)SFSET_GetCond(sfd, SFD_COND_PESOBJ); fn = (void *)SFSET_GetCond(sfd, SFD_COND_PESFN); MPS_SetPesFn(mps, fn, obj); }`
  called in place of the second `MPS_SetPesFn(mps, GetCond(FN), GetCond(OBJ))` line. The helper's locals are inlined `@N` webs, so `mr @t,r3; mr obj,@t` /
  `mr fn,@t` are COMPILER copies: ghost list 9 -> 11 (`r66->r4=@677` = fn onto its argument register, `r88->r67` = the r3 copy into obj's web), chaitin IDENTICAL,
  cnt r21, bufin/dst r21. Evaluation order matters: `fn` first = 4w (`li r4,0x5b/0x5c` swapped + `mr r4,r3`/`mr r5,r22` swapped); `obj` first (= the argument
  list's right-to-left order in the original) = IDENTICAL. The first (PSMAP) pair stays inline (a helper for both pairs = 127w/139w: +4 ghosts, the PSMAP pair's
  named-web bounce). Pin `asm { mr r31, err; mr ret, r31 }` removed (`ret = err;`), `n = (len < 0xB0) ? len : 0xB0`. Catalogue row 2 addendum: a call result
  that must stay a codeless coalesced copy (a ghost) is an inlined helper's LOCAL (`obj = f(); g(obj)` inside a `static` helper), not an own local (user copy,
  `mr` kept) and not a plain argument expression (no copy at all).
- **adx_tsvr `adxt_nlp_trap_entry` 2w (`lha r4` vs `r0` in B16): the what-if is `edge=56:<any r0-coloured node coloured before r56>` (e.g. `edge=56:67`, or
  `ghost=56:0`) -> r4 with nothing else moving.** The lha temp r56's real neighbours are r1, n2's r3 ghosts, ofst2v r26, ofst1 r27, n1 r28, sji/sjd/p; r0 is free.
  The target needs an r0 node live in B16 (`lha; cmpwi n1; lha ofst2v; add`) that leaves NO instruction: not a dead def (dead defs have degree 0 in the graph:
  r52-r54 in sfh_main are `0/0`), not an argument ghost (r3-r10 only), not a value across the else-arm's call (r0 is a physical neighbour of those). Helper
  forms of the n2 join (`n2 = adxt_nlp_scan2(n1, &ck2, &ofst2)` with an if/else local, an early-return body, `+ 0` on the result) = 2w each: the helper's n2 web
  coalesces onto the `@ret` r3 exactly like the own local (frontend-01 shows the if/else already as one ECOND -> one backend temp r54 + `@ret` r55 -> r3).
  `if (n1 != 0) if (n2 != 0)` 2w; `(n1 & n2) != 0` 11w (`and.`); `(Sint16)(Uint16)ofst` casts 13/14w (clrlwi/extsh emitted); `ofst2v = ofst2` first 2w. Left 2w.
- **sfh_main `SFH_AnlyElemSmpHz` 6w: the colour side is `edge=36:4` (the word r36 = @894 needs physical r4 = `id` live at the load) -> r6 exactly; the fold side
  is NOT a block-flag question.** The post-RA peephole processes CLEAN blocks too: the asm-chain form (`register w, s; SFH_SWAP32_STORE` without `peephole off`)
  leaves B37 `000c` through the RA (no dead terms, no copies to coalesce) and pass 15 still folds it to `stwbrx` (flags -> 0004 after the peephole). Also folded:
  the asm chain with the `stw` inside the asm (`register Sint32 *val`), `mr t, s; stw t` inside the asm; an asm `nop` after the stw stops the fold but reshapes
  the whole function (65w). So in this compiler the fold fires on any contiguous `rlwinm; rlwimi x3; stw` whose chain reads one register, and the six helper
  readers' `#pragma peephole off` remains the only blocker. The target's r6 = the same "one more blocked register" as the helpers' (there hdr r5, the dying
  base; here r4 = id, NOT the dying base r3 which `li r3,1` reuses), i.e. a node live across the swap block that the final code does not show. Left 6w.
- **mwsfdcre `mwPlyCalcWorkSfd` 4 -> 0w APPLIED (tagged, not flipped: CreateSfd 115w).** Form: `register Sint32 size` declared ABOVE rfbsiz/tabsiz, the chain
  unchanged, then `size += MWSFD_FNAME_SIZE; asm { mr size, size } return sibsiz + size;`. Reading: (1) the identity asm copy IS a second rvalue read for
  the frontend's pull count, so the last `size` web stays a variable and `return sibsiz + size` is computed straight into r3 (`addi r3,r3,0x4800; add r3, r29,
  r3`, no `@ret` copy for the RA to delete -> B15 stays `000c` -> the epilogue `lwz r0` is not hoisted); the `mr r39,r39` itself is deleted by backend-02
  copy propagation, before scheduling (codeless, no dirtying, no ghost — unlike the pass-63 neighbour pin whose copies live to the RA). (2) With the chain
  kept, `size` is a real node coloured with the own locals: declared after rfbsiz/tabsiz it is coloured last (r5, they r3/r4 = 13w); declared before them
  it takes r3 first and they r4/r5 = the target (own locals colour in declaration order within the level). Pure-C second reads all fail: `size = size`,
  `+size`, `size + 0`, `* 1`, `| 0` are folded by the frontend (13w); `size = (Sint32)size` / `(Sint32)(size + 0)` / `(Sint32)(Uint32)size` KEEP the read
  but create a new web (the constant adds fold into it: `addi r0, r3, 0x4800; add r3, r29, r0`, 2w; placed after `size = size2 + adxwksiz` 3w). Helper
  forms (the chain in a `static` helper with 7/8 parameters, `return sibsiz + helper(..)`, `return sib + size` inside, or `size = helper(); return sibsiz +
  size`) collapse the whole chain and reassociate (13w): inlined helper webs are pulled like own locals. Tree: src/lib/mwsfdcre.c mwPlyCalcWorkSfd only.
- **mwsfdcre `mwsfcre_CreateSfd` 115w (the two dead `b T` of the inlined IsUseAdxt case-4 arm): not addressed.** Arm contents that the frontend deletes
  (`mode = mode`, `(Sint32)mode`, `mode + 0`, `if (mode) break;`) 115w; `mode = 4; return TRUE` 120w (+8, a second `li 1`), `return (Bool)(mode == 4)` 133w;
  a hoisted `Bool ret = TRUE` with `case 4: ret = TRUE` (backend-CSE candidate) changes the tree to a dominating `li r0,1` without the `li 0; b; li 1` diamond
  (125w, -8). The target's diamond = `return FALSE`/`return TRUE` @ret arms plus an emptied case-4 block: its statement survived the frontend and was
  deleted by a backend pass before the RA (pass 63) — still no C statement found that the frontend keeps and backend-01..05 delete.
- **mpv_umc `mpvumc_OneReadMb` 48w: not addressed.** Colour read: the target's `lwz r25` vx dies at `clrlwi yhx` BEFORE `lwzx fn_y` (fn_y then reuses r25) and
  `clrlwi chx` is issued AFTER both table loads (eec), the reverse of ours (chx before fn_y, yhx after) = the pass-49b c33 tie; vx L2 (33 nbs) in ours. Probes:
  `mc->src2 = mc->src + cpitch + chx` 59w (+4), `yhx &= mcflag` right after its def 48w, `chx = ((Uint32)cvx & 1) & mcflag` 53w, the same for yhx 77w, cvx/cvy
  right after the vector loads 61w, the whole body as a `static inline` helper behind a wrapper (`#pragma inline_max_size`) 71w. Left 48w.

### CRI pass 66 (mwsfdcre Matching 8 -> 10/10 FLIPPED, 111 OK: mwsfcre_CreateSfd 115 -> 0w — the two dead `b` are the emptied case-4 arm of the inlined IsUseAdxt laid out BEFORE the FALSE arm; frontend/backend deletion census of arm contents; 2026-09-12)
Harness ~/.cache/cri66/ (deleted at the end): `gen.py NAME 'case-4 body' [--decl|--pre|--post|--ret|--params|--glob|--order 4first]` (a
25-line TU: `static Bool IsUse(mode)` switch inlined twice into `Create`, the CreateSfd shape), `try.sh`/`batch.sh` (ra.py dump -> `blocks.py DUMPDIR
--passes 0,6,14` = one line per block of the tree region after each backend pass, the frontend-01 CASE labels), `bytes.sh` (the GC/2.7 production
command on the probe, no strip step, llvm-objdump of the tree = the bytes judge; ra.py's pcode dumps do NOT show the emitter's branch threading),
`real.py NAME 'IsUseAdxt text'` (the tree's unit with the helper replaced, variant.sh).
- **Correction to the task's premise: the target is the LARGER one (0x2c28 vs ours 0x2c20; CreateSfd 0xe70/0xe68)** — the 115 words are the two
  extra `b` plus every branch offset behind them; nothing else differs, so the "2 extra words of register residue" do not exist.
- **The mechanism, read in full.** Target tree at both sites: `cmpwi m,4; beq T; bge T; cmpwi m,2; bge F; b T; b T; F: li 0; b E; T: li 1; E: cmpwi r0,1`.
  The first `b T` is the tree's own fall-through branch (m < 2 -> default), the second is the CASE-4 ARM'S BLOCK reduced to its `b T`, laid out between
  the tree and the FALSE arm because the source had `case 4:` FIRST. The `beq` goes to T (not to the block) because the object emitter threads a branch
  to a `b`-only block but does not delete the unreachable block (the pcode dumps still show `bt B7` -> `B7: b B9`; the bytes show `beq T`). Two
  conditions, both necessary: (1) the arm was NOT empty at the frontend — an empty arm (`case 4: break;`, or anything the frontend deletes) is folded
  onto the default label (`CASE 0x4: L@26 = DEFAULT: L@26` in frontend-01) and no block exists; (2) the block sits where its successor is not the
  fall-through. Pass 60's negative ("a codeless pin in the case-4 arm does not leave the `b`") was condition (2): with `case 4:` written after the
  MPV/VONLYSFD arms the emptied block is laid out right before T and falls into it — the same pin with the arm first gives the `b` (probe: identical tree).
- **What the frontend deletes (census, frontend-01 label test):** every def of a local or parameter that is dead afterwards, however spelled (`x = mode`,
  `x = mode / 2`, `x = (Uint32)mode >> 31`, `register` locals, `mode = 4`, `mode = 0`, `mode++`, `mode = mode`, `{ Sint32 m = mode; mode = m; }`), a def
  overwritten on every path before its use (`x = 0` then `x = mode` after the switch: liveness-based, not "no use at all"), `(void)mode`, `mode / 0`, a
  two-level inline whose body folds (`mode = Id(mode)` with `Id` returning its parameter; `r = Id(mode)` with `Id` returning TRUE and `r` redefined after
  the switch), plus pass 7/65's list (unreachable statements, empty blocks, labels, `if (mode) break;`). Kept: a volatile read (`x = *(volatile Sint32 *)&mode`
  -> a real `stw`/`lwz`), a store, a call, an `asm` statement, and a LIVE def (a def whose value reaches a use).
- **What the backend deletes before/at the RA (the arm then keeps its `b`):** (a) `common-subexpression-elimination` (a pass the pipeline adds only when
  the codegen emitted duplicate expressions; it runs at backend-06 in the probe, 02 and 10 in CreateSfd) deletes a def identical to a dominating def
  of the SAME vreg with no redefinition between: form A `Bool ret = TRUE; case 4: ret = TRUE;` — the case-4 `li r39,1` is gone after pass 06 while the
  init `li r0,1` is scheduled into the tree's first block (target has no `li` before the tree, so the vendor's deleted def was not this); (b) the RA
  deletes coalesced copies — `asm { mr r11, mode; mr mode, r11 }` (`mr r11,r36; mr r39,r11` until pass 08, gone after 09) and the plain self-copy
  `asm { mr mode, mode }` on a `register` parameter (one `mr r36,r36`, deleted at the RA, no physical register reserved, no ghost: every colour of
  CreateSfd unchanged). Dead defs never reach the backend (frontend), so candidate (a) "pre-RA dead-store elimination of a register local" does not
  exist as a backend event; (b) overwritten local = frontend; (c) a memory dead store is never deleted (code); (d) `case 4: case 5: X` shares one
  block — no separate `b`; (e) an inlined helper that folds = frontend; (f) copy-propagated arm values: a user copy the frontend keeps is live and leaves
  its `mr`. No pure-C statement was found that the frontend keeps and the backend deletes for free here: the only dominating instruction available
  at both sites without a call between is the tree's own `cmpi cr0,mode,4` (calls kill the `cprm->mode` load: `mode = cprm->mode` in a macro-form arm
  is reloaded, `lwz r30,0(r29)`), and a compare statement always keeps its branch (pass 7).
- **Applied (tagged, `COMPILER-DIFF: M1 (codeless arm)`):** `static Bool mwsfcre_IsUseAdxt(register Sint32 mode)` with `case 4: asm { mr mode, mode }
  break;` as the FIRST arm, then `case MWSFD_FTYPE_MPV: case MWSFD_FTYPE_VONLYSFD: return FALSE; default: break;` + `return TRUE;`. variant.sh IDENTICAL
  on the first try; tree object rebuilt under the lock, bytecmp IDENTICAL (10/10), `"lib/mwsfdcre.c": True` in objects.py (`# CRI pass 66` block),
  `flock ... ninja -k 0` + `dtk shasum -c` = 111 OK. Tree edits: src/lib/mwsfdcre.c (IsUseAdxt + header comment), config/G4BE08/objects.py.
- Lever catalogue: MWCC row added (the last row) — "a dead unconditional `b` in the target that ours lacks". Rule for any unit: when the target has
  a `b` to the join that ours lacks and the function is 4 bytes shorter per `b`, look for a switch arm (or if-arm) that the frontend folded away in
  ours; the original's arm had a statement, and its POSITION in the source decides whether the emptied block needs a branch.
- Note on the shared tree: `ninja` printed "premature end of file; recovering" (a clobbered .ninja_deps from an unlocked run elsewhere) and re-split
  once; the locked full build settled (main.dol built, 111 OK on two consecutive checks).

### CRI SWAR kernels pass 19 (mpv_mcy 16x16 H2 197w / V2 225w; IN PROGRESS 2026-09-12)
Harness ~/.cache/cri_swar19/ (swar18's scripts + `tlive.py SBS L1 L2` = the target's values/live ranges/degrees from a `variant.sh --all`
listing, `sigmatch.py RA_DIR SBS RANGES` = role map ours-vreg -> target register by expression signature for all four cases, `gsearch.py`
= chaitin.py score of a dump against the target's colours with a hill-climb over the own-local order and the `@N` group order, `tgraph.py`
= case-0 what-ifs (own/CSE slots, extra edges), `cases.sh NAME` = per-case differing-line counts; bodies/, NOTES.md).
- **H2 case-0 role map (target register -> role):** w0 r21, w1 r10, w2 r12, w3 r30, a0 r22 (`mr r22,r7; rlwimi r22,r21`), a1 r11, a2 r29,
  byte b r20 (`lbz r20,0x10; mr r8,r20; rlwimi r8,r30` = a3 pack r8), x0 r24, x1 r28, x2 r31, x3 r7, i r9, m1 r5, m2 r6, srwi bases r7/r8,
  sums r23-r27/r10/r11. Ours (tree): w1 r22, w2 r9, w3 r10, a0 r27, a1 r30, a2 r11, a3 r7, b r12, x1 r29, x2 r12, x3 r28, i r8.
- **The difference is the KIND of the x's, not a live range:** chaitin.py on the tree's own graph reproduces 41/44 case-0 colours once
  x0..x3 and the a3 pack web sit ABOVE the `@N` webs in the scan order (frontend CSE temps) with the own locals `w0,a0,w1,a1,w2,w3,a2` and
  the byte `a3` lowest; no permutation of own locals alone exceeds 11/17. Spelling: `AVG2X(w, a, m1, m2)` = `((w) & (a)) + ((((w) ^ (a))
  & m1) >> 1) + (((w) ^ (a)) & m2)` — the frontend CSEs the duplicated `(w ^ a)` into one @temp per pair (numbered after all `@N` webs,
  x0 highest) — with no `x` variable in case 0: 197 -> 170w (A1), + `a2` declared before `w3`: 167w (A2), case 0 = every colour but the
  byte (ours r25, target r20). The byte takes r20 in the target because r20 is already HANDED when it pops (case 1's `w0'` web has r20 in
  the target; ours hands r20 to nothing), so the byte's colour is a numbering fact of case 1, not of case 0.
- Target association in cases 2/3 is the mask-VARIABLE one (`(w & a) + (sh + (x & m2))`, `add sh,xm2; add wa,..`): the tree's literal
  `MPVMC16_AVG2` there rebuilds `sh + ((w&a) + (x&m2))` — wrong shape (A5 fixes the shape; words 173 because the colours move).
- Cases 1/2 in the model: with every `@N` group order and own-local order free, A5 reaches 102/161 role colours (38/42, 19/43, 18/43, 33/39),
  A6 (CSE x's in all four cases) 124/161 (40/42, 28/43, 27/43, 35/39): the case-1/2 graphs still differ from the target's (open).

### CRI pass 67 (adx_dcd5 Ste4AsMono 143w read fresh in the model: the tree's Mono is SEMANTICALLY WRONG (pass 44's `r1 = sc_r*q; r1 += (c1*t + c2*r1) >> 12` reads the NEW r1 in the prediction; the target reads the old r1 = `mullw r26,r10,r30`), the target's level structure found as a what-if (20/22 named colours); IN PROGRESS; 2026-09-12)
Harness ~/.cache/cri67/ (`wi.py RA_DIR --names [fixed=VID:K edge=A:B ghost=VID:PHYS ghostphys=P:PHYS vid=VID:KEY del=VID]` = chaitin what-if with
name-based target colours, `trace.py RA_DIR [edits]` = per-scan degrees of every long-lived node + picks, `combo2.py RA_DIR` = grid over grouped
never-removed additions, `t.sh NAME [--ra]` = variant.sh words + ra.py dump of `mono_NAME.c` spliced into base.c; deleted at the end).
- **Role map of the Mono target (register -> value):** sadd r0, c1e r9, c2e r10 (extsh in place), scl r11, l2 r12 (!), nblk r18, smul r19, body temps
  r20/r21 only, magic 0x66666667 r22, qtbl r23, sc_r r24 (= key's register in the frame loop), sc_l r25, dr r26 (= the ctr `li 0x10`), d r27, i r28,
  l1 r29, r1 r30, r2 r31; in place: c2*l2 -> r12, c2*r2 -> r31, c1*l2 -> r27 (d dead), c2*l1 -> r26 (dr's index dead), q_r*sc_r -> r21. `mr r31,r21`
  (r2 = t) kept because t (r21) and r2 differ; the mix m is ALSO r21 (t dead at the mr) -> the frontend did NOT propagate t into `c1 * r2`, so t is
  REDEFINED between the copy and the use: the vendor reuses `t` as the mix variable (`r2 = t; t = (l2 + t) * 7 / 10; ...; t = (l1 + r1) * 7 / 10`).
- **Colouring order = declaration order l2, r2, r1, l1, i, d, dr, sc_l, sc_r (the Ste order) in ONE level after sadd/scl, with qtbl/magic/T's a level
  below and smul/nblk spill picks.** Scan structure that reproduces it (trace.py): scan 1-2 remove the body temps; the graph is then STUCK -> pick
  nblk (cost 17), still stuck -> pick smul (33/deg, deg(smul) > deg(sadd)); scan A removes q_r, q_l, the mix web, the surviving `c1*l1`/`d>>4`-type
  temps (vids below the magic addi), magic (r22) and qtbl (r23) — they are visited AFTER the own locals/c1e/c2e, which stay >= 29 at their visit;
  scan B removes sc_r, sc_l, dr, d, i, l1, r1, r2, l2, c1e, c2e (ascending vid -> coloured c2e r10, c1e r9, l2 r12 = lowest free, r2 r31 ... sc_r r24);
  scan C = scl, sadd (sadd r0 first by vid, scl r11 because it is adjacent to physical r9/r10). The colours are NOT "handed out in colouring order"
  everywhere: key reuses r24 before sc_r, the ctr `li` reuses r26 (dr).
- **What-if that scores 20/22 on the Ste-shaped correct-semantics variant (mono_b: t reused as the mix, q_l/q_r locals, unsplit r1):** +2 never-removed
  neighbours on EVERY node adjacent to r6 (two ghosts live across the whole function = the histl/histr copies of Ste's requirement 1) AND +3 on the
  three stack parameters sadd/smul/scl (or +2 on all three and +1 more on sadd, e.g. the argument-base ghost r73 -> r1 which today is adjacent to
  scl/smul but not to sadd because sadd's load is its last use). Razor-thin: all+1 or all+3, or sadd+2, give 5-8/22. Remaining misses: q_l/q_r
  swapped (q_r r21 must be coloured before q_l r20: `q_r` declared before `q_l`, or q_r removed in the later scan) and the spurious `li 0x10` pick.

### CRI pass 68 (mpv_umc Matching 15 -> 16/16 FLIPPED, 111 OK: mpvumc_OneReadMb 48 -> 11 -> 0w pure C — the chroma halves are frontend CSE @temps (`((vx / 2) >> 1)` evaluated inside the cpos expression, read off the target's srawi/XER order) and `mbx8` a two-use own local declared last; 2026-09-12)
Harness ~/.cache/cri68/ (o2g.py = interference graph rebuilt from a hypothetical pre-RA order + chaitin colours, validated edge-exact on
ours; search_stmt*.py = sched.py model on statement-group permutations x declaration orders scored against the target's registers;
try.sh NAME [--dump]).
- **Hard evidence read off the target's final code: the six `srawi` are serialised by the model's XER (class-0) WAW chain in RAW order,
  pre-RA and post-RA alike, so their final order IS the vendor's statement order.** Target: `vx>>1, vy>>1, cvx, cvx>>1, cvy, cvy>>1`;
  ours (own locals `cvx = vx / 2; cvy = vy / 2;` then cpos): `vx>>1, vy>>1, cvx, cvy, cvx>>1, cvy>>1`. Only an expression that evaluates
  `(vx/2) >> 1` before `vy/2` gives the target's order: `cpos = ofs[0] + ((vx / 2) >> 1) + ((vy / 2) >> 1) * cpitch; chx = (Uint32)(vx / 2)
  & 1; fn_c = tbl_c[(vy / 2) & 1][(vx / 2) & 1];` (the frontend CSEs `vx / 2` / `vy / 2` into @temps @182/@183, created in the cpos
  statement). 48 -> 11w (harness v2): the whole vector block is byte-identical (vx r25 / vy r11 / cvx r28 / cvy r7 / yhx r24 / chx r23).
  Mechanism in the model: cvx/cvy as @temps have vids above every own local, so they pop before vx/vy in L1 (cvx takes r28 = ypos's
  register: all volatiles and r23-r27 taken by its neighbours, ypos not one); vx (L1, 28 neighbours now that the c-chain is issued after its
  death) pops with all volatiles taken and r23 (tby+ r75, live across vx) / r24 (yhx: vx has a read after `rlwinm yhx` in the new
  schedule) covered -> r25 = fn_y's register. The pass-49b/65 "yhx before fn_y" reading was right about the colours and wrong about
  the lever: no DAG edit on the y/c chains was needed, the whole vector block re-ordered itself once cvx/cvy were @temps.
- Corollary for the catalogue: the pass-49b "c33 tie" was a symptom; the tie never needed flipping. The srawi/XER chain is a general
  order oracle: any `srawi`/`addic`/`subfc` (XER writers) in a CRI target appear in statement order.
- Remaining 11w = the top block: mby r10 / mbx r11 (ours r11/r12), `mbx*8` r12 (ours r10): the target's `mbx*8` interferes with BOTH
  `mby*16` and `mbx*16` (defined before `add ofs0 = mbx8 + mby8*cpitch`), ours only with mby*16. Statement/declaration search 54/56 max.
- **Second half, 11 -> 0w: `mbx8 = mbx * 8` as a TWO-USE own local declared last.** Target top block: mby r10 / mbx r11 / `mbx*8` r12; ours
  `mbx*8` r10 (a single-use `mbx * 8` is substituted by the frontend and becomes a backend temp r57, coloured BEFORE mby/mbx in L1 with r10
  the lowest free volatile, pushing mby/mbx to r11/r12). Model reading (o2g.py, sched.py trace of the completion queue): the target's r12
  needs r10/r11 covered at `mbx*8`'s colouring; the "interference" route (both `mby*16`/`mbx*16` issued before `add ofs0`) is unreachable
  — the `mullw mby8*cpitch` holds IU1 for c3-c7 and sits at the head of the 6-entry in-order completion queue, so c5/c6 have ONE IU pick
  each (lis oneref / addi mc win on frees) and c7/c8 none; no statement or declaration order gives it (search_stmt2 54/56 max). The
  "colouring-order" route works: with `mbx8` a real variable (two uses: `ofs[0] = mbx8 + mby8 * cpitch; ofs[1] = mbx8 * 2 + mby16 *
  rfb->ypitch;`, the `* 2` folded by peephole-forward back into `slwi mbx,4` reading mbx) declared LAST, its vid is the lowest own local,
  it pops after mby/mbx, which hold r10/r11 -> r12; then mby r10 / mbx r11 follow. `mbx8 << 1` identical. IDENTICAL 16/16 in the harness
  (v4/v5) and in the tree.
- Flip: `"lib/mpv_umc.c": True` (objects.py `# CRI pass 68` block), locked `ninja -k 0`, `dtk shasum -c` = 111 OK. Tree edits:
  src/lib/mpv_umc.c (OneReadMb body + its comment, file-header OPEN line), config/G4BE08/objects.py, AGENTS.md. No tagged form; the unit's
  remaining asm bodies are the paired-single kernels (vendor inline asm, START HERE rule).
- Method notes for the kit (harness deleted; recipes here): (1) `o2g.py` = liveness over a hypothetical pre-RA order -> B1's interference
  edges patched into `chaitin.load()`'s graph (edge-exact against the dump on ours; drop dead defs = vregs never read, keep the dump's
  physical/other-block edges); with `sched.schedule_block()` in front it turns "statement order x declaration order" into a colour score in
  ~10 ms per candidate — 20k-candidate hill-climbs find the ceiling of a lever class in a minute (26/43 -> 32/43 for statement+declaration
  on the old form = the pass-31/63 sweeps' negative result, reproduced without compiling). (2) The srawi/XER (class-0 WAW, lat 1) chain is
  a statement-order oracle in both scheduler passes. (3) `rw` bit 4 in the sched-pre2 operands (`R4:25:5`) appears on last uses only (not used by the model).
### CRI pass 67, part 2 (continuation of "CRI pass 67" above — the CRI pass 68 and 2-word ties sections landed between; adx_dcd5 Ste4AsMono 143 -> 62w APPLIED with correct semantics + one r6 neighbour pin; Ste4AsSte 25w unchanged; not flipped; 2026-09-12)
- **APPLIED (src/lib/adx_dcd5.c ADX_DecodeSte4AsMono 143 -> 62w, Ste4AsSte 25w unchanged, sizes equal, locked ninja + bytecmp 2/4; objects.py
  untouched, no flip):** the Ste-shaped body (mono_d4): declaration order `l2, rr2, rr1, register l1, i, d, dr, Sint16 sc_l, Sint16 sc_r, s, t,
  nblk, key, j, q_l, q_r`; Ste's scale block (`sc_l = ((s ^ key) & 0x1FFF) + 1; key = sadd + key * smul; *scl = key; *scl &= 0x7FFF`) so the
  scales stay own locals; the body `l2 = (d>>4)*sc_l + pred; t = (dr>>4)*sc_r + pred; rr2 = t; t = (l2 + t) * 7 / 10; CLAMP(t); q_l = AdxQtbl[d&0xF];
  outr[0] = t; q_r = AdxQtbl[dr&0xF]; outl[0] = t; l1 = q_l*sc_l + ((c1*l2 + c2*l1) >> 12); rr1 = q_r*sc_r + ((c1*rr2 + c2*rr1) >> 12); t = (l1 + rr1)
  * 7 / 10; ...` (correct semantics: the old rr1 in the prediction, no split), and ONE tagged neighbour pin `asm { mr r6, l1 }` after the hist
  loads (M1, as Ste's). Pin census on the same body: r6 on l1 62w, on l2 64w (= r8 on l2), on rr2 66w; r6+r8 pins 111-113w (too much pressure:
  sadd becomes a pick); pins placed BEFORE the hist loads 178w; no pin 164w. q_r declared before q_l: 62w (no change). The pure-C correct body
  without a pin is 164w (mono_b) / 227w with a separate `m` (mono_a).
- **Residue 62w read in the model (chaitin --k 28 on the d4 dump: picks `li 0x10` r148, nblk, smul; scl r11 top; the cascade level coloured c2e r0,
  c1e r9, l2 r12, rr2 r31, rr1 r30, l1 r29, i r10, d r28 ... sadd r24):** sadd sits one level too low. `fixed=42:3 fixed=41:2` (three more
  never-removed neighbours on sadd, two on smul, smul's degree staying above sadd's so smul is the pick) -> 20/22 named colours (only q_l/q_r
  r20/r21 swapped remain). sadd+3 alone or sadd+2/smul+1 make sadd the pick (11-12/22). In ours smul/scl are adjacent to the argument-base ghost
  r73 (-> r1) and to physical r1 while sadd is not (its load is the base's last use); the target's pick order needs deg(smul) > deg(sadd) by
  exactly one or more with sadd raised by 3. Not found in C: the stack loads stay in parameter order whatever the expression order (`key * smul
  + sadd` identical), own-local copies of histl/histr (`hl = histl`) and of c1/c2 (`c1e = c1`: 240w) create no ghost, an inlined `sethist(h, a, b)`
  helper for the final stores creates none, `-O4` / `-O3` / `-O2` / `-O4,s` / `-inline off` all move away (only `-O4,p -inline auto` keeps Mono4/
  Ste4 identical). Modelled negative: c1e/c2e as coalesced parameter webs (extsh in place on a multi-def parameter web) score 2-6/22.
- **Correction to the pass 44 part 3 record:** the 180 -> 143w "split" `r1 = sc_r * AdxQtbl[dr & 0xF]; r1 += (c1 * t + c2 * r1) >> 12;` changed the
  semantics (ours multiplied c2 by the NEW r1: `mullw r25,r31,r20; mullw r18,r29,r25` vs the target's `mullw r26,r10,r30` on the old r1). Word counts
  of a colouring device must be checked against the operand roles, not only the instruction pattern. Removed from the tree by this pass.
- Catalogue note (MWCC row "two values with swapped registers", picks): when the target hands the LAST callee-saved registers to two cheap
  long-lived values (nblk r18, smul r19 here) they are Chaitin spill picks — the graph was stuck with every alive node >= 29 — and the pick order is
  cost/degree with the degree deciding equal costs (33/deg for both Sint16 stack parameters). Use trace.py-style per-scan degrees before adding
  neighbours: the target's structure needed sadd/smul ABOVE the loop values by +3/+2 and everything else by exactly +1 (the r6 pin), and any
  other distribution (+2 all, +2 on the prologue values, +1 on sadd) lands 5-12/22.
- Harness ~/.cache/cri67 and /tmp/cri67f deleted. Tree: src/lib/adx_dcd5.c ADX_DecodeSte4AsMono only. adx_dcd5 2/4 (25w + 62w), not flipped.
- **game/db_cam `debugCamera::menu` 2w (unchanged, the four enumerated shapes tested once each on the campos copy, /tmp/ties/dbc):** raw-word
  copies `*(u32*)&d0[k] = ((u32*)&campos)[k]` (d1: 60w, the copy leaves move_by_pieces), a one-member struct view `VecSlot* d0; d0->v = campos`
  (d2: 46w, size -4), a `"=m"(*(u32*)&campos.y)` anchor after the copies = the "fourth 4(r9) use" (d3: 31w, a store to the static flushes the
  block), a `"=m"(pos.y)` anchor (d4: 10w). Reading refined with the db_widget arithmetic: closer 7's "y-first sched1 world is unallocatable" holds
  only for a 2-ref y — with y issued first AND the target's store order (z's store before y's) y is [30,42) 2 refs = 1666 < z 3333, but at 4 refs
  (floor_log2 2) it is 6666 and takes r0 first; so the target's sched1 order can be y-first if y has two extra sched1-only reads. Such reads must
  name the y WORD of the memcpy expansion, which C cannot (any word-copy spelling replaces the expansion). Not closed; nothing applied.
- **lib/adx_tsvr `adxt_nlp_trap_entry` 2w (unchanged; the mandated MWCC shapes tested once each, /tmp/ties/tsvr):** helper whose LOCAL is the
  B16 value — `ofst1 = adxt_nlp_add(ofst1, ofst)` with `return a + b` (t1), with a local `v = b` (t2), with `Sint16 *pb; v = *pb` (t3), with an
  extra parameter copy `m = n1` and `+ (m & 0)` (t4), a `void` helper writing `*pa += v` (t5): all 2w — the helper local is coalesced into the
  lha temp and still colours r0 (lowest free; nothing live in B16 is r0). Dead initialisers `ofst1 = 0` / `n2 = 0` / all four (u1-u3): 2w (no
  range-split web of these variables reaches B16 with a new number); reversed declaration order (u4): 8w (ofst1 r26 / ofst2v r27); a second
  `ofst1 = ofst1` def (u5): 2w. Left 2w. Next (not this pass): MWCC excludes r0 from a web that is ALSO used where r0 is illegal (an `addi`/
  load base); the target's r4 could be a web of `ofst` shared with such a use that the final code folded — check frontend-01 for a second read.
- Flags: `t_esp/db_widget.cpp` True (the `# 2-word ties pass 1` block in config/G4BE08/modules.py), game/db_cam and lib/adx_tsvr unchanged.
  Built under the lock: build/G4BE08/src/t_esp/db_widget.o, t_esp.rel, then `ninja -k 0`; `dtk shasum -c` = 111 OK; symbols.txt unchanged.
  Tree edits: src/t_esp/db_widget.cpp (DB_STRING ctor + `primIdCounter` linkage), config/G4BE08/modules.py, this section. Kit untouched.
### CRI pass 69 (adx_dcd5 Ste4AsMono 62w / Ste4AsSte 25w: the srawi/XER order oracle read on both — it MATCHES ours statement by statement, so the residue is not expression order; model readings below; IN PROGRESS; 2026-09-12)
Harness ~/.cache/cri69/ (`wi.py RA_DIR [--k N] [--ste] fixed=VID:K edge=A:B ghost=PHYS ghostlike=VID vid=OLD:NEW del=VID` = chaitin what-if scored against the target role map; `t.sh NAME [--ra]` = variant.sh words + ra.py dump of NAME.c; deleted at the end).
- **srawi/XER oracle (method 1): Mono target body order `d>>4, predL>>12, dr>>4, predR>>12, mix>>2, predL1>>12, predR1>>12, mix2>>2`; prologue `nblk` srawi; ours IDENTICAL at every offset (fdiff shows only ARG_MISMATCH lines, no INSERT/DELETE/REPLACE in the body). Ste: the three body srawi and the nblk srawi at identical offsets too.** `add rD, rA, rB` operand order also matches (`(d>>4)*sc_l + pred`, `sadd + key*smul`, `c1*l1 + c2*l2`, `mulhw magic, x`). So the vendor's evaluation order = ours; both residues are pure colouring (a register permutation of the same instruction multiset), and method 1 yields no lever here. `mulhw` does not write XER (only `srawi`/`srawi.`/`addic`/`subfc`/`addc`/`adde`... do); the body's `mulhw` are ordered by their srawi consumers anyway.
- **Mono residue re-derived on the pinned tree dump (`wi.py ra_mono --k 28`, 10/20 baseline, `fixed=42:3 fixed=41:2` = pass 67's 18/20 reproduced):** at sadd's scan-B visit its degree is exactly 25 = 13 never-removed (physical r3..r10 + coalesced src/nfrm/outl/histl/outr webs) + scl + 9 own locals + c1e + c2e; the target needs >= K there while keeping deg(smul) > deg(sadd) at the stuck point (equal costs 33; the pick tie goes to the higher vid = sadd, so smul must be strictly ahead — in ours it is ahead by exactly the r1/r73 argument-base pair that sadd lacks because its stack load is the base's last use). Any never-removed neighbour added to sadd alone closes that gap: sadd +2 via `edge=42:1 edge=42:73` (the Ste requirement 2) makes sadd the pick (5/20). Grid on the UNPINNED pure-C dump (ra_u, 164w, no picks at all, everything in one level): 18/20 needs [ghosts on r6+r8 (+1 on every loop value)] AND (sadd+3, smul+2) | (sadd+4, smul+3) ...; OR [ghosts on r6, r8, r9, r10] AND sadd+1 (smul+0) — a ghost on r9/r10 is adjacent only to the values live at the c1/c2 `extsh` in B1 (scl, smul, sadd, nblk, l1, rr1, rr2, l2, c2e): a kept parameter copy of c1/c2 dying at its extsh. Uniform pressure (3-7 ghosts on r6) never works (picks change).
- **Alternative reading with the same 18/20 and the right picks (nblk then smul): `vid=42:66.5 ghost=6 ghost=8` — sadd's LOOP NODE is a value with a vid above the c1e/c2e @temps (visited last in scan B, pushed last, coloured first -> r0; c2e then takes r10 in place, c1e r9, l2 r12, the cascade follows), plus the two whole-function ghosts.** The natural such node is the frontend's hoisted `(long)sadd` @140 (vid r67 > @141's r66): the backend turns its `extsh r67,r42` into `mr r67,r42` at backend-09 (constant propagation knows the `lha` is sign-extended) and folds it at backend-11 (both single-def), leaving the parameter r42 as the node. This reading does NOT transfer to Ste (there sadd must be visited BEFORE the own locals and survive: pass 62's r1/r72 adjacency gives sadd r0 / smul r11 / scl r12 exactly), so the shared-shape hypothesis still favours "sadd +never-removed neighbours" — but for Mono that must come with smul +2 so that smul stays the pick.
- Negatives this pass (unpinned base, words / ghosts): `Sint32 co1 = c1, co2 = c2` own locals declared first 113w (size equal: the `mr r31,r21` survives) / declared last 240w; `register` on the c1/c2/smul/sadd parameters 164w (= base). Ghost list unchanged in all three (`src nfrm outl outr + base->r1`).

### CRI SWAR kernels pass 19, part 2 (continuation of "CRI SWAR kernels pass 19" above — other agents' sections landed between; mpv_mcy 16x16 H2 197 -> 22w APPLIED (cases 0 and 3 byte-identical), V2 225 -> 199w APPLIED; the case-1/2 mechanism = the original's interference graph is the SOURCE-ORDER live ranges, reached by variable reuse; not flipped; 2026-09-12)
Harness ~/.cache/cri_swar19/ (adds `valid.py RA_DIR SBS RANGES CASE` = is the target's colouring a proper colouring of OUR graph (conflict list),
`cmpx.py OUTDIR BLOCK RA_DIR SBS RANGES CASE [--coal --nodead --k6 --serx --nosched --split=K --valid --dump]` = sched.py pre-RA order of a
block under DAG edits diffed against the target through the sigmatch role map, plus the conflict check on the MODEL's order, `lvl.py`
= chaitin what-if with extra never-removed neighbours (levels), `declclimb.py FUNC BODY` = greedy climb over the declaration order,
`cases.sh NAME [V2]` = per-case differing lines by target address; bodies A1..N6/VA1..VD1 = the probes named below; NOTES.md).
- **The decisive test (valid.py): the original's case-1/2 colourings are NOT proper colourings of our interference graph (x1/x2/x3 all
  r27 while ours keeps x1 live when x2 is defined; x0m1/x1m1/x2m1 all r23), but they ARE proper colourings of the SOURCE-ORDER live
  ranges (`cmpx.py --nosched --valid`: 0 conflicts for every DAG variant).** The pre-RA scheduler hoists every pair's xor/and to the
  block top (all four x's live at once); the original's schedule keeps pair k+1's values after pair k's uses. Variable REUSE gives it
  in C: the frontend's range-split webs of ONE variable redefined per pair (`a0 = ..pair 0..; d[0] = ..; a0 = ..pair 1..; d[1] = ..`)
  are separate vregs, but the scheduler keeps their statement order (D3/D4: 0 conflicts, 197 -> 113w with `x0`/`a0` reused in cases 1/2).
  Neither the K6/asm leftover-free DAG (`--coal --nodead`: 4 conflicts), a single >100 block split (`--split=K`: 3-9), nor
  `#pragma scheduling off` (source order but NO post-RA schedule either: 277w) gives the original's order/graph.
- **Numbering facts read this pass (all confirmed by the real compiler):** (1) a dead initialiser (`Uint32 b = 0`) does NOT make the
  first real definition an `@N` web — the own-local node is still the first REAL web (B2: copies into `b0..b3` kept); so a pack whose
  copy must coalesce (`srwi rD; rlwimi rD` with no `mr`) is a variable first defined in an EARLIER case (case 0's w0..w3/a0..a3), and a
  variable first defined in case 1 is an own local there (its loads: v0..v4). (2) Own locals of a later case pop LAST of everything
  (lowest vids): the original's case-1 words at offsets 0/4 = r22/r23 and V2's r17/r18 are such loads. (3) The `@N` group order is by
  first definition (case 0's statement order); within a reused variable later definitions pop first; the case-1/2 pack webs need
  the pop order pairs 2, 3, 1 in BOTH families (r8/r9/r10 and r11/r12/r31), which the mapping pair k -> `w[(k+2)&3]` / `a[(k+2)&3]`
  gives (L8, = L2's `w0,w3,w0,w2 / a0,a3,a0,a2`); the natural k -> k order pops 1, 2, 3 (H5 57w), one reused variable pops 3, 2, 1
  (J1 44w). (4) Case 0's byte `a3` declared FIRST (highest own vid) makes it pop before w0/a0 -> r20 (E1, case 0 identical).
- **Applied form (src/lib/mpv_mcy.c H2):** `MPVMC16_AVG2X(w, a, m1, m2)` = the xor inside the macro (frontend CSE temp per pair, no
  x variables anywhere); declaration `a3, w0, a0, w1, a1, w2, a2, w3, v0, v1, v2, v3, v4`; case 0 as before; cases 1/2 `v0..v4 =
  W(s,0..16)` then per pair `w[(k+2)&3] = pixel; a[(k+2)&3] = neighbour; d[..] = AVG2X(w, a)`; case 3 unchanged (loads into a0..a3,
  packs w0..w3, AVG2X). Words 197 -> 170 (A1: CSE x in case 0) -> 167 (A2) -> 113 (D4) -> 106 (E1) -> 65 (G2: v-loads + a0 reuse) ->
  57 (H5: CSE x) -> 44 (J1: w0/a0 reuse) -> 22 (L8). Left (22w): cases 1/2 rotate the v2/v3 loads and pair 1's neighbour in r29-r31
  (the original pops v2, v3 before pair 1's neighbour = an own local declared after v2/v3 whose pack has no copy; every C in-place form
  for it (mask complement, `|=`, `__rlwimi`, N1's `(v2 >> 16) & 0xFFFF`) leaves a `mr` or fuses the other operand; `register` + asm
  rlwimi moves every own local (139w)), and case 2's v2/v3 pop order (target v3 first there, v2 first in case 1: a level difference).
- **V2 (225 -> 199w APPLIED = VA1 + x0 reused in cases 1-3):** case 0 (CSE x, decl `w0,a0,..,a3,w4,a4`) = the original's colours except
  w0/a0 (r28/r30 for r17/r18: the original's case-1 loads at offset 0 are own locals popped last, handing r17/r18 first) and w2..a3
  one level too high (L2 in ours, popped before the x2m1 temp). The H2 recipe (loads into own locals p0..p4/q0..q4, w0/a0 reused: VD1
  220w; distinct w_k/a_k: VC1 230w) does NOT transfer as is: the original's V2 case 1 has the loads at offset 4 (`lwz r8`) volatile =
  an own local at L3, the byte packs in place on the `lbz` (`w4 = (w3 << 8) | w4` or the coalesced copy) and eight distinct pack
  registers; its role map is in NOTES.md — next pass.
- Catalogue rows (MWCC table): (a) "the target's colouring is not a proper colouring of ours' graph (valid.py), x's of successive
  pairs share a register" -> the original's pre-RA order = source order per pair -> reuse ONE variable per role across the pairs
  (range-split webs keep statement order in the scheduler); (b) "a pack's base written straight into the variable in a later case
  while the first case keeps `mr`" -> the variable is first defined in the first case (@N web later), loads that are volatile-free
  own locals are first defined in that case; dead initialisers do not move the own-local web; (c) "L2 pop order of a family of
  webs = pairs 2,3,1" -> index the variables (k+2)&3.
- **New hard fact from the target's B1 colours (Mono): the pre-RA order had `srawi nblk` BEFORE `lha l2` and AFTER the `lwz scl`/`lha sadd` stack loads.** The `nfrm/2` add temp r75 is r12 in the target = l2's register, so r75 and l2 do not interfere (l2 loaded after the srawi that kills r75); r75 is not r0/r11, so sadd (r0) and scl (r11) ARE live at r75's death (their loads precede the srawi); the `srwi` temp r74 is r0, so sadd's load is after the `add`. In ours (pinned and unpinned) the srawi is issued at c6-c8 as a SECOND pick after `lha l2` (the srawi has height 1 and frees nothing; loads have height 2; the class-0 parameter `mr`s of src/outl/outr win the second slots of c3-c5 on the opcode-class tie-break), so r75 interferes with l2 and takes r0. Verified with sched.py --post: replaying ours' post-RA B1 input with the target's colours substituted gives a broken order (the srawi sinks under the l2 load), while a pre-RA order with the srawi issued between `lha l1` and `lha l2` (config (i) below) reproduces the target's final B1 INSTRUCTION FOR INSTRUCTION (stwu, srwi, add, extsh c2e, stmw, lis, lis, srawi, lwz scl, extsh c1e, lha smul, addi, lha sadd, addi, li, lha l1, lha l2, lha rr1, lha rr2, b). Pre-RA configurations (schedwhatif on the tree's dump, block 1) that satisfy scl/sadd < srawi < l2: (b) no parameter `mr`s at all (srawi c4 second pick after `lha sadd`); (i) the histl copy `mr r35,r6` kept (the l1/l2 loads read it) with NO histr copy and no pin (srawi c6 after `lha l1`, l2 c7); (f)/(q) a consumer copy of nblk (`mr rX,r47`: frees 1 -> srawi c3) TOGETHER with consumer copies on all three stack loads (else the srawi runs ahead of the scl/sadd loads: (a)/(e)). Negatives: histl+histr copies (j, any insertion position: the extra frees-1 mr delays the rlwinm chain a cycle), c1/c2 copies (n)/(o), stack-load copies without the nblk copy (r)/(s)/(u)/(v)/(w) — all put the srawi at c7-c11 after l2. So the vendor's B1 had FEWER frees-0 class-0 `mr`s competing for the second issue slots than ours, or a consumer of nblk that vanished before the final code.
- **RA side, consolidated (wi.py, unpinned dump ra_u, K=29): the target's 18/20 (all named colours except q_l/q_r) is reached by exactly two families:** (A) `ghost=6 ghost=8` (two whole-function ghosts = histl/histr coalesced copies) + `fixed=42:3 fixed=41:2` (never-removed neighbours on sadd/smul only) — pass 67's reading; or (B) `vid=42:66.5 ghost=6 ghost=8` (sadd's loop value is a node with a vid ABOVE the c1e/c2e @temps + the two ghosts); `ghost=9 ghost=10` (c1/c2 copies dying at their extsh) may be added to (B) freely, `vid=41:66.6`/`vid=40:66.7` (smul/scl as high-vid nodes) too. (B) with ONE ghost (the tree's histl ghost, K=29) scores 16/20: the entire cascade sadd r0 / c2e r10 / c1e r9 / l2 r12 / rr2 r31 .. sc_r r24 / scl r11 right, only smul (r23: not a pick) + q_r/s/t off. (B)'s natural node is the hoisted `(long)sadd` @140 = vid r67 (vids = 207 - @; @141 c2e = r66): the backend turns `extsh r67,r42` into `mr r67,r42` (backend-09) and folds it (backend-11), so in ours the parameter r42 (vid 42, visited FIRST in scan B) is the node. Every C copy of sadd (`Sint32 sa = sadd` own local first/last/`register`, `*scl = sadd + *scl * smul` without the key local) compiles to the same 164w object (frontend substitution). The q_l/q_r 2-word tail is the mix temp @148 (r59) being coloured before q_l (r44) and taking r20 (the target's mix is r21, q_l r20); a separate mix local `m` (declared before or after q_l) is 227w unpinned (pass 67's mono_a confirmed).
- Ste read with the same tools: its srawi order matches too; `vid=42:66.5` on the pinned Ste dump puts sadd in a lower level (r31), so family (B) does not transfer to Ste, whose target needs sadd visited BEFORE the own locals and surviving scan B (pass 62's r1/r72 adjacency = requirement 2) — the two functions' residues have different mechanics under this model unless the vendor's Ste/Mono shapes differ in the same place (sadd's node).
- Tree unchanged (adx_dcd5 2/4, Ste 25w + Mono 62w, both pins as found); objects.py untouched; nothing built under the lock. Harness ~/.cache/cri69 deleted. Left for the next pass: a C shape that (1) keeps the histl copy but not histr's (or removes the src/outl/outr `mr`s from B1's second slots) and (2) gives sadd's loop node a vid above the @temps — both readings point at the parameter-copy/argument-setup layer of the vendor's source, not at the loop bodies (which are now oracle-verified in evaluation order).
- **Final state of the pass:** src/lib/mpv_mcy.c H2 197 -> 22w (L8 form), V2 225 -> 194w (VB1 + `a3` declared before `w2`: the declclimb.py
  result; `a3`'s first web = case 1's `a3 = W(s1,12)` load... the own-local order moved 5 words), 4p 30w unchanged; mpv_mcy 2/5, not flipped.
  V2 probes this pass: own-local loads p0..p4/q0..q4 with distinct w_k/a_k (VC1 230w), with w0/a0 reused (VD1 220w), with the (k+2)&3
  rotation (VE1 218w), with the fifth loads packed in place `p4 = (p3 << 8) | p4` (VE3 203w, case 1 74 -> 51 lines: the original's
  `lbz r22; rlwimi r22,r24` IS that form). sigmatch.py now names the two load bases s0/s1 by first use, but the V2 role map still leaves
  ~99 values unmatched (the sums' operand order through two bases) — repair it first next pass, then run valid.py per case as for H2.
- Harness: ~/.cache/cri_swar18 DELETED; ~/.cache/cri_swar19 kept small (scripts, bodies/, NOTES.md; dumps and objects deleted).

### CRI pass 70 (adx_dcd5 Ste4AsMono 62w / Ste4AsSte 25w: the parameter-copy / argument-setup layer; IN PROGRESS; 2026-09-12)
Harness ~/.cache/cri70/ (`wi.py RA_DIR [--k N] [--ste] ghost=P ghostlike=VID:P fixed=VID:K vid=OLD:NEW edge=A:B del=VID` = chaitin what-if scored against the Mono/Ste role maps by NAME (QTBL/MAGIC/C1E/C2E named from the pre-RA pcode); `grid.sh RA_DIR [--k N] -- "edits".. `; `mk.py NAME [--ste] 'OLD=>NEW'..` = base.c with exact-string edits inside ONE function; `t.sh NAME [--ra] [--sd] [--ste]` = variant.sh words + ra.py/scheddump dumps; deleted at the end).
- (H1, read off the target) the target's B1 has NO parameter `mr` in the final code and the loop reads src/outl/outr/histl/histr/c1/c2 in r3/r5/r7/r6/r8/r9/r10 = the same coalesced-ghost set as ours (`src nfrm outl outr`); the in-place `addi r3/r5/r7` steps prove the same multi-def parameter webs. Model reproduced on the unpinned dump (ra_u0, K=29): `ghost=6 vid=42:66.5` 16/22, `ghost=6 ghost=8 vid=42:66.5` 20/22 (= `ghost=6 ghost=8 fixed=42:3 fixed=41:2`), ghost 8 alone = ghost 6 alone. Pinned tree dump (K=28) + `vid=42:66.5` = 19/21 (q_l/q_r only).
- (H2 probes, unpinned Mono, all no r6 ghost): `l1 = *histl++; l2 = *histl;` + `histl[-1]/[0]` epilogue stores 113w size equal (the post-increment makes l1 a COPY of the load temp `lha r76,r6; mr r55,r76` = a frees-1 consumer in B1, srawi moves to between rr1/rr2; the addi is folded ACROSS blocks into the epilogue stores); `*histl++ = l1; *histl++ = l2;` (trailing dead addi) 164w = base; `*histl++ = l1; *histl = l2;` 164w; `l1 = *histl++; l2 = *histl--;` 153w size +4. K&R-style definition (`()` prototype) = byte-identical to the ANSI form (164w; the stack shorts are still `lha 0x52`).
- Mono4 (matching, 0w) re-read for contrast: `chaitin.py --check` IDENTICAL, ghosts `src nfrm out + base->r1` (no hist ghost); its `(long)smul` widening of the REGISTER parameter r10 is the top-scan r0 node (`extsh r0,r10`), while the stack parameter sadd (`lha r30,0x2a`) colours with the own locals. In Ste4AsMono/Ste the STACK parameters sadd/smul/scl are the top-scan nodes — as if their widening copies (`mr @140,r42` / `mr @139,r41`, folded by backend-11 in ours) had survived to the RA (family (B) = the coalesced pair's leader r67 > c2e r66; the same copies are the frees-1 consumers that pass 69's config (f)/(q) needs in B1).

### CRI SWAR kernels pass 20 (mpv_mcy 16x16 H2 22 -> 0w IDENTICAL pure C APPLIED, V2 194 -> 173w APPLIED (all four cases admit the target colouring; case 3 = the same-variable form with one L3 load, read in the model); mpv_mcy 3/5, 4p 30w parked, not flipped; 2026-09-12)
Harness ~/.cache/cri_swar20/ (swar19's scripts with paths fixed + `gen.py NAME BASE decl=.. 'cN:OLD=>NEW' 's/OLD/NEW/'` = body edits, `run.sh NAME [V2]` = words
+ per-case lines (RA=1 adds the ra.py dump), `roles.py RA SBS RANGES CASE [--all]` = per-node ours/target colour + pop index, `wif.py RA move=NAME:POS
coal=KEEP:GONE edge=A:B` = chaitin what-if with vid moves and copy coalescing; bodies/, NOTES.md; deleted at the end).
- **H2 22 -> 0w (T3 = the pass-19 L8 form + two edits): pair 1's neighbour in cases 1/2 is written into `a1` (the SAME variable as pair 3's
  neighbour), and case 1 loads `v3` before `v2`.** Read: valid.py gave 0 conflicts on the L8 graph in every case, so the residue was pure pop
  order: target case 1 v2 r29 / v3 r30 / n1 r31, case 2 v3 r29 / v2 r30 / n1 r31. The model (wif.py) shows n1 must pop after v2/v3 at an OWN-LOCAL
  vid with its base copy coalesced — impossible: copies into or out of an own local never coalesce (Q3 `n1 = (v1<<16)|(v2>>16)` as an own local keeps
  `mr n1,t` with the temp at r8; P1's two-statement in-place form splits the second def into a new `@N` web and keeps `mr @web,n1`; a single-use `n1`
  read inside the AVG is substituted by the frontend (R1); `register`/`Sint32` change nothing). So n1 is a web, and the hand-out rule decides the
  rest: a web popping when NO callee-saved register has been handed out yet takes r31 (new hand-out, r31 downward). n1 must therefore pop BEFORE the
  case-3 loads of a1/a2/a3 (the groups that hand out r31/r30/r29) and AFTER pair 3's neighbour (r12): as a web of `a1` defined EARLIER in the case than
  pair 3's `a1` it has the higher @ inside a1's group = pops right after it (T1, which put v2/v3/n1 into a1/a2/a3's groups, popped them in group
  order and handed r31/r30/r29 the wrong way round). v2/v3 stay own locals of case 1 (popped after every web: r29 = lowest free handed-out, r30);
  case 2's v2/v3 are `@N` webs numbered by FIRST DEFINITION, so the case-1 load statement order `v3, v2` pops v3 first there (Q2: the schedule of
  case 1 is unchanged by the statement order — the loads are issued by the DAG). Two numbering facts for the catalogue: (5) a web's colour among
  the callee-saved registers depends on WHICH groups popped before it (hand-out from r31 downward happens once per register; later webs take the
  lowest free handed-out), so a value that must take r31 in a later case pops before the earliest hand-out of the function's later-case webs;
  (6) two defs of one variable inside one case: the earlier def has the higher @ and pops LATER than the later def.
- **V2 194 -> 173w APPLIED (VT5): cases 1-3 load the ten words into OWN locals of the case `p1,p2,p3,p4,q1,q2,q3,q4,p0,q0` (declared after
  the case-0 variables), with `p0 = W(s0,0); s0 += stride; q0 = W(s1,0); s1 += stride;` AFTER the other eight loads — a single-use load is
  substituted by the frontend into its pack and becomes a backend temp (VT1: `lwz r158` unnamed), the pointer step right after it keeps it a
  variable (VT3) — the six packs into the case-0 variables `w0,a0,w1,a1,w2,a2` written BEFORE the four AVG stores (per-pair order put pair 0's
  `srwi` before pair 2's rlwimi and kept p2 live across it: valid.py 1 conflict, the target's `srwi r28` reuses p2's register; all-packs-first
  gives 0 conflicts), the fifth load packed in place in cases 1/2 (`p4 = (p3 << 8) | p4` -> `lbz r22; rlwimi r22`) and into w3/a3 in case 3
  (the target's case 3 has `srwi r22, r18(p4)` separate). Case 0 9 -> 5 lines, case 1 64 -> 42, cases 2/3 70/74. valid.py: 0 conflicts in all
  four cases — the residue is pop order only.**
- **sigmatch.py repaired for V2:** the target's masks are found by the nearest `lis rN, 0xfeff` / `lis rN, 0x101` before the loop (V2 keeps m1
  in r3 = the dead `mc` register and m2 in r7; H2's r5/r6 were hard-coded), and `ranges.py SBS` computes the per-variant loop line ranges (V2
  listings shift with INSERT/DELETE lines). 47/47/49 case values now map (99 unmatched before).
- **Numbering read off the VT5 dump (frontend-02 + regalloc names):** (a) the p/q own locals' case-2/3 webs form groups in the case-1 STATEMENT
  order p1,q1,p2,q2,p3,p4,q3,q4,p0,q0, each group [case 3, case 2] (case 3 lowest @, popped first); (b) a variable redefined per pair inside a
  case (the old `x0`) does NOT get its own-local node from the first case: VT4's `x0` node r43 was CASE 2's pair-0 xor, and case 1's four defs were
  `@206..@209` with the EARLIER pair the LOWER @ — the opposite of the H2 rule (two defs of one variable in one case: earlier def = higher @);
  the frontend's web numbering for a variable with several defs per case in several cases is not the simple first-definition rule and must be
  read off the dump per variant. (c) Case 3's seven loads p1,q1,p2,q2,p3,q3,p0 are L3 in ours (degree >= 29 at scan 2: visited LAST in the scan
  because case-3 webs have the highest vids of their groups) and take r3,r7..r12; the target's case 3 has p1 r31, q1 r29, p2 r27, q2 r25, p3 r24,
  q3 r23 (coloured AFTER W0/A0/W1 r10-r12 and interleaved with the packs) and only p0 volatile (r8).
- **Model result for case 3 (vsearch.py, free permutation of the 23 `@N` webs of the case on VT5's graph: 49/49 reachable, so the graph is right):**
  the pop order that reproduces the target is p0 (alone in L3 -> r8), then W0 r10, A0 r11, W1 r12, x0, then pack-before-load PAIRS: W1' -> p1 r31,
  A1 -> q1 r29, W2 -> p2 r27, A2 -> q2 r25, then q3, p3, A3 r22 -> q4, W3 r21 -> p4, i.e. the pack that inserts load k pops right before load k:
  the numbering of the SAME-VARIABLE form (`w1 = W(s0,1); ... w1 = (w1 << 24) | (w2 >> 8)`: within a case the later def = lower @ = pops
  first) — the pass-19 VB1 form for case 3, which in ours colours wrong only because its loads land in L3. Cases 1/2 of the target have the loads
  BEFORE the packs (q1 r30 then A1 r29, p2 r28 then W2 r27, q2 r26 then A2 r25) = different variables for load and pack (VT5's form) with the
  callee-saved hand-outs happening in that interleaved order; case 1/2 colours differ from case 3 exactly in those three pairs plus p1/p0/q0/x0.
  Own-local declaration order alone tops out at 95/182 (case 1 38/47; cases 2/3 unchanged at 14/47, 6/49).
- Next pass (V2 173w): (1) case 3 in the same-variable form (VB1's) while cases 1/2 keep VT5's, and make case 3's loads L2 — the L3 status is a
  scan-order effect (case-3 webs are visited last in scan 2); test in the model by moving the case-3 load webs below the pack webs (wif.py move=)
  before writing C; (2) then the case-1/2 hand-out order (x0/p1 r31 first, q1 r30, A1 r29, p2 r28, W2 r27, q2 r26, A2 r25, p3 r24, q3 r23, p4'
  r22, q4' r21, q0/x1 r18, p0 r17 = one new register per pop, so the target pops these in exactly that order with every earlier one blocking).
  Do not repeat: VE3-style packs-first without own-local loads (203w), per-pair statement order (VT3, the p2 conflict), the (k+2)&3 rotation of
  H2 on V2 (VT2 222w), own-local declaration climbs (ceiling 95/182).
- Tree: src/lib/mpv_mcy.c H2 (T3) + V2 (VT5) + their comments; mpv_mcy 3/5 (4p 30w parked: its Uint8-pair / a8 >= 29 requirement was not touched —
  nothing in the H2/V2 findings suggests its form). objects.py untouched, no flip, nothing else built. ~/.cache/cri_swar19 DELETED;
  ~/.cache/cri_swar20 kept small (scripts, bodies/, NOTES.md; dumps deleted) for the V2 pass — delete it there. Kit untouched.

### CRI pass 70, part 2 (continuation of "CRI pass 70" above — the SWAR pass 20 section landed between; adx_dcd5 Ste4AsMono 62 -> 26w APPLIED: sadd read through its own address (`ps = &sadd; key = *ps + key * smul`, pure C) + the r6 pin moved after the loop; Ste4AsSte 25w unchanged; not flipped; 2026-09-12)
- **APPLIED (src/lib/adx_dcd5.c ADX_DecodeSte4AsMono only; locked ninja + bytecmp: Ste 25w, Mono 26w, sizes equal, objects.py untouched):** `Sint16 *ps;` declared last, `ps = &sadd;` after `nblk = nfrm / 2`, both key updates `key = *ps + key * smul`, and the existing `asm { mr r6, l1 }` (M1) moved from before the frame loop to after it (before the hist stores). Census: ps alone with the pin in place 28w; ps + pin after the loop 26w; ps without any pin 93w; pin after the loop without ps 178w (= the unpinned 164w + the ghost's level shift).
- **(H3) mechanism = the address-taken stack parameter.** With `&sadd` used, the parameter is not loaded into its web at entry (no `lha r42,r73,0(sadd)`, no `(long)sadd` @temp): every `*ps` read is a load of the caller's slot, the frontend substitutes ps, and backend-05 loop-code-motion hoists the invariant load into the preheader as a BACKEND temp (`lha r79,r72,0(sadd)`: vid above the c1e/c2e @temps r65/r66) — family (B)'s node, `wi.py ra_b3 --k 28` 19/21 (q_l/q_r only), picks nblk/smul as the target. Dead address-takes (`(void)&sadd`, an unused `Sint16 *p = &sadd`, `*(&sadd)` inline, an inlined `rd(&sadd)` helper, `Sint32 sa = *ps` into an own local) are all simplified back to the parameter (178w = base); only a `*ps` read in the key expression survives. The same lever on Ste is negative as pass 69 predicted (sadd 91w, smul 89w, scl 83w, with/without the pin move): Ste needs sadd in the top scan by DEGREE, not by vid.
- **(H2) the r6 pin's copy is the B1 issue-slot problem, not the histl copy.** A pin anywhere outside B1 (frame-loop top, loop end, after the loop) keeps the histl copy `mr r35,r6` (the asm's write of r6 still blocks the physical propagation) and B1's pre-RA order becomes pass 69's config (i) exactly (`mr mr rlwinm lis add lis lwz mr lha mr lha mr lha srawi lha ..`: srawi between `lha l1` and `lha l2`); on the pinned graph `vid=42:66.5` scores 20/22 (K=28). Placement inside the loop costs nothing extra (the copy is deleted by the RA as before) but the after-loop position is the one that leaves the loop blocks untouched.
- **(H1/H2 negatives, unpinned Mono, 164w = base unless noted):** `key = key * smul + sadd`, `nblk = nfrm / 2` after the hist loads, `const` stack parameters, `while` frame loop; `sadd = (Sint32)sadd` at the loop end (148w, +8 bytes) / loop top (179w) / before the loop (134w, an `extsh` stays); K&R definition; `asm { mr sa, sadd }` into a `register Sint32 sa` (the asm copy is folded by backend copy propagation like a C copy: only a copy INTO a physical register survives); hard pin `asm { mr r0, sadd }` after the loop 35w / in B1 37w (r0 leaves the colour set: `srwi r11` + a third spill pick, frame +0x10). Widening copies `mr r67,r42`/`mr r68,r41` inserted in the schedwhatif model do NOT reproduce the target's B1 either (srawi c8) — the target's B1 is config (i), reached now.
- **Residue 26w (Mono), read:** (a) `add r12,r0,r4` = the `nfrm/2` add temp r75 coloured r0 in ours: the hoisted sadd load sits LAST in B1's input order (preheader temps after the hist loads), so sadd is not live at the srawi; the target needs sadd's load before the srawi (the stack-parameter position) while keeping the high vid — a frontend-hoisted load would do it, not found; (b) the two `lis` pairs swapped (magic r20/r21): the AdxQtbl address temp is created before the 0x6666 magic in the target's IR order (table lookups placed before the first mix in the source: c1-c4 probes 47-229w, none keeps the body); (c) the q_l/q_r tail (mix temp @147 coloured before q_l; `q_r` declared first, inlined lookups d1-d4 26-36w: unchanged or worse).
- Tree: src/lib/adx_dcd5.c ADX_DecodeSte4AsMono (body + comments). adx_dcd5 2/4 (25w + 26w), not flipped, no `ninja -k 0`. Harness ~/.cache/cri70 deleted.

### CRI pass 71 (adx_dcd5 Ste4AsMono 26w / Ste4AsSte 25w: the hoisted sadd load cannot reach the parameter position — loop code motion appends to a NEW preheader block; the target's pre-RA B1 derived from its colours = all three stack loads issued before the `add`, both `lis` before the `srawi`; the coalescing direction read off a dump (member = the copy's SOURCE); nothing applied, nothing flipped; 2026-09-12)
Harness ~/.cache/cri71/ (`wi.py RA_DIR [--k N] [--ste] [--all] [--trace VIDS --nbat SCAN] fixed=VID:K ghost=P ghostlike=VID:P edge=A:B unedge=A:B vid=OLD:NEW del=VID` = chaitin what-if scored against the Mono/Ste role maps by name (SADD/MAGIC/QTBL/QLIS/MIX/NBADD found by pcode pattern), `--trace` = per-scan degrees + picks, `--nbat` = the alive neighbours at one scan; `mk.py NAME [--ste] [--src F] 'OLD=>NEW'..` = base.c with exact-string edits inside ONE function; `t.sh NAME [--ra] [--sd] [--ste]` = variant.sh words + ra.py / scheddump dumps; `mkpost.py NAME SD_DIR <schedwhatif edits>` = the pre-RA order that schedwhatif produces for B1 under the edits, MR copies dropped, the Mono target colours substituted, real stwu/stmw prepended -> `fake_NAME/` for `schedwhatif.py fake_NAME 0 --post` = the post-RA replay of a hypothetical pre-RA B1 with the target's registers; deleted at the end).
- **Mono (a) `add r12,r0,r4` is unreachable with a backend-hoisted load.** backend-05 loop-code-motion creates a NEW preheader block (B51) between B2 (the statements before the loop) and the loop header and appends the hoisted pcodes there: first the inner loop's invariants (the two magic and two AdxQtbl `lis/addi` pairs in body order; CSE'd to one pair each at backend-14), then the outer body's invariant loads (`lha sadd`); a second address-taken parameter (`pm = &smul`, m1/m1b 32w) is appended AFTER sadd's in body order (`*ps` before `*pm`), and both become high-vid temps (smul r79 pops before sadd r78 -> smul r0, sadd r19: the pick structure is lost). B1+B2+B51 are scheduled as one block: the seven loads tie (height 2, no successor) and issue one per cycle in INPUT order, so the hoisted load is always the last load; a consumer on it (`extsh`/`mr` inserted after it in schedwhatif) makes it height 3 / frees 1 and it jumps to c1 AHEAD of `lwz scl` (the target has scl, smul, sadd). Statement position of `ps = &sadd` (before nblk, after the hist loads: m2/m3) and one plain `sadd` read + one `*ps` read (m4/m5) all compile to the SAME 26w object: an address-taken parameter has no web, every read of it (spelled `sadd` or `*ps`) is a slot load. So the address form gets the LEVEL right by accident (a high-vid node) and can never get the position; the target's node is at the parameter position (7th pcode of B1, after `lha smul`) with a vid above the c1/c2 widenings (@140/@141 = r66/r65).
- **The target's pre-RA B1, derived from its colours (both functions):** Mono's add temp r12 = l2's register while sadd is r0 and scl r11 -> at the `add` the sadd AND scl loads are already issued and l2's is not (the srawi precedes `lha l2`); Ste's add temp r19 (nblk's own register: nothing volatile free) -> all three stack loads (r0/r11/r12) precede the `add` there. In ours (p0 = plain unpinned Mono, sd_p0) the add is at c2 and the first load at c3 (c0-c2 second slots go to the frees-1 `lis` pairs and the h3/h4 parameter `mr`s). schedwhatif configurations that satisfy the constraints: (D) consumer copies on ALL THREE stack loads + one on nblk (`ins=15:MR r200,r40` ...: loads c0/c1/c2, add c2 first slot, `lha sadd` c2 second, srawi c3, l1 c4, l2 c5) and (noMR-nolis) no parameter `mr` and no `lis/addi` in B1 (loads c0/c1/c2, srawi c2 second slot). Consumers on sadd alone (A) or smul+sadd (B) put those loads at c0 ahead of scl. **mkpost.py replay of (D) with the target's registers reproduces the target's final B1 instruction for instruction EXCEPT `srawi` vs the two `lis`: replay `stmw, srawi, lis, lis, lwz r11 ..`, target `stmw, lis, lis, srawi, lwz r11 ..`** — the post-RA tie (both h2) goes to input order, so in the vendor's pre-RA OUTPUT both `lis` precede the `srawi` (pre-RA: `srawi` frees-1/h2 ties with `lis` frees-1/h2 and wins by class/input order in (D); the vendor's `lis` pairs sit BEFORE the nblk chain in the input, i.e. not appended by loop code motion after B2, or the srawi has frees 0 and something delays `lha l1/l2`). Three things the vendor's B1 has that ours lacks: frees-1 consumers on the three stack loads (coalesced away before the final code), `lis` earlier than the nblk chain, and (Mono) sadd's node above the widenings.
- **Coalescing direction read off a dump (SFHDS_SetHdr `lbz r54; mr r44,r54; rlwimi r44 ..`): r54 (the copy's SOURCE) is the `fCoalesced` member, r44 (the multi-def DEST) the leader.** Parameter moves `mr r32,r3` are the exception only because r3 is precoloured. Hence a surviving widening copy `mr @140,r42` makes @140 (vid 67 > c2e's 66) the leader = pass 69's family (B) exactly, with r42 a ghost adjacent to the B1 values only, and the entry `lha r42` at the parameter position with a frees-1 consumer = (D)'s sadd consumer; a surviving `mr r50,r76` (Ste `qtbl = AdxQtbl`) makes the own local r50 the leader = pass 62's requirement 3. Both copies are folded in ours by backend copy propagation (03/11) because source and destination are single-def; they survive only when the DESTINATION is multi-def (SetHdr's rlwimi chain) — no C spelling of a multi-def widening/own-local pointer without extra code found this pass (`sa = sadd` twice: the second copy interferes; `qtbl = AdxQtbl` twice: pass 62's +8).
- **Ste in the model (u0 = the tree's Ste with the pin removed, 115w, K=29):** `ghost=6 ghost=8` 16/21; + `edge=42:1 edge=42:72` (requirement 2) puts sadd r0 / smul r11 / scl r12 right but drops to 9/21 because QTBL (r76, the addi temp) pops first in L8 -> r31 and shifts every own local by one; `--trace`: at scan 7 QTBL has degree 29 = K exactly (l2 28, sc_r 26) — it is the only long-lived node adjacent to physical r0 (an `lwzx`/`addi` BASE is excluded from r0 by an edge to physical r0: src/outl/outr/scl/i/r72/QLIS/QTBL have it, sadd/smul/l1/l2/... do not; that is why the target's sadd can be r0 and qtbl cannot) and is visited after the own locals (vid 76) -> removed last in scan 7. The target's QTBL r22 (popped right after sc_r r23, before nblk r19 / t r20) needs it removed in scan 6 (degree 33 -> 28) or at a vid below sc_r (51) with degree <= 28 at scan 7 (`vid=76:50.5` alone keeps it at 29 -> top scan -> r11): the own-local leader r50 of a surviving `mr r50,r76` has fewer B1 neighbours than r76 (r76 dies at the copy) — requirement 3 re-derived on the unpinned graph.
- **Frontend hoisting of a loop-TEST invariant:** `for (i = 0; i < nfrm / 2; i++)` (no nblk local) makes `nfrm/2` an @temp (vid 64, below the widenings) placed after the for-init and the c1/c2 `extsh` in B1's input, before the backend's B51 hoists; on the plain form (m7) 164 -> 100w with the add temp r20 (all volatiles live at the late add), on the tree form (m6) 26w unchanged; Ste (s6) 26w. A statement-position lever for the nblk chain, not the target's order.
- Pragma sweep on the plain Mono (p0, 164w; the pragma placed before the function applies to Mono4 too): `optimization_level 3` = 164w (identical to -O4), `optimization_level 2` 200w, `1` 280w, `optimize_for_size on` 240w, `opt_lifetimes off` 130w, `opt_loop_invariants off` and `global_optimizer off` 112w (c2's widening becomes a separate `extsh r0,r10`, Mono4 42w), `opt_propagation/opt_common_subs/opt_dead_code/opt_dead_assignments/opt_strength_reduction/opt_unroll_loops/opt_vectorize_loops off` no effect at all (164w) — the pass-62 observation confirmed: these pragmas are accepted and ignored by 2.4.7.
- Other negatives: `Uint16 smul, Uint16 sadd` parameters with `(Sint16)` casts at the use (m8): `lhz + extsh` real instructions (+8 bytes) and the caller changes; `pm = &smul` (m1) 32w.
- Tree unchanged (adx_dcd5 2/4, Ste 25w + Mono 26w, pins as found); objects.py untouched; nothing built under the lock. Harness ~/.cache/cri71 deleted at the end of the pass. Next: a C shape whose backend copies into the widening @temps / the qtbl own local survive (destination multi-def without code), or a shape that emits the `lis/addi` pairs before the nblk chain in B1's input (a table/magic use in a statement before `nblk = nfrm / 2` that the frontend keeps).
- **Late probes (all negative):** Mono with `qtbl = AdxQtbl` as a statement (own local, any declaration/statement position: m9/m10/m11) 95w — the folded copy leaves the addi temp r75 live across B1 (issued c6), it survives scan 13 and pops first in the own-local level (r12, cascade); Ste `qtbl = AdxQtbl` moved before `nblk = nfrm / 2` (s8) or after the hist loads (s9) 26w (+1); Ste `qtbl = AdxQtbl + 1; qtbl--;` (s7) 162w +4 (the `addi -4` stays: lwzx has no offset to fold into). **Mono (b) read off the temp numbering:** in ours the kept magic pair is `addi r108` (the division root's constant temp, allocated BEFORE the mix's child temps r109-r111) + `lis r112` (materialised after the children), the kept table pair `lis r116 / addi r117` (consecutive); backend-14 CSE keeps the FIRST occurrence of each pair in B51 (mix1, table1). The target's colours need vid(magic addi) < vid(qtbl addi) AND vid(magic lis) > vid(qtbl lis), i.e. the table address temps allocated between the mix root's constant temp and its materialisation — the lookup would have to be a child of the mix expression, which the XER-fixed statement order excludes; no pair-order or CSE-choice (mix2's pair kept: addi r137 > r117) satisfies both. Unexplained; 4 words.
- Final state: nothing applied (adx_dcd5 2/4, Ste 25w + Mono 26w as found); ~/.cache/cri71 deleted.

### CRI SWAR kernels pass 21 (mpv_mcy 16x16 V2 173 -> 139 -> 131w APPLIED: same-variable form in ALL three cases + the fifth pair packed in place in case 3 + case 3's p0 a substituted load; the hand-out rule re-read: "lowest free handed-out" = the MOST RECENT hand-out; mpv_mcy 3/5, 4p 30w untouched, not flipped; 2026-09-12)
Harness ~/.cache/cri_swar21/ (swar20's scripts with paths fixed + `mkcase.py NAME BASE CASE loads=.. packs=.. steps=mid|end avg=..` = rewrite one case's loop
body, `gsearch2.py RA SBS RANGES --webs` = hill-climb over the vid order of ALL own locals + `@N` webs scored over the four cases, `feas.py` = per-node
necessary conditions of the target colouring under the pop rule, `why.py RA SOLUTION nodes..` = replay a gsearch2 solution and print each node's used /
handed sets and which neighbour covers which callee-saved register; bodies/, NOTES.md; deleted at the end).
- **Rule correction (matters for every later-case reading):** chaitin.py's colouring takes the lowest-numbered free register among r0,r3-r12 AND the
  handed-out callee-saved set — and since hand-outs go r31 DOWNWARD, "lowest handed" = the most RECENTLY handed-out register. A node of a later case
  that pops with all volatiles covered therefore takes the register of the function's LATEST hand-out (not r31), unless its coloured neighbours cover
  it. Pass 20's "one new register per pop" for cases 1/2 was wrong: cases 1/2 make NO hand-outs at all; case 3 hands out r31..r21 in its L2 pop order
  (p1, A1, q1, W2, p2, A2, q2, p3, q3, W3, A3 = the same-variable numbering: pack then load per variable, groups in case-0 definition order w1,a1,w2,a2,
  w3,a3 then p4,q4), and the case-2/1 webs of the same variables pop right after their case-3 counterparts within each group and take the latest
  hand-out: W1c2 r12, p1c2 r31 (handed set {31}), A1c2 r29 ({31,30,29}: r29 latest), q1c2 r30 (r29 covered by A1c2), W2c2 r27, p2c2 r28, A2c2 r25,
  q2c2 r26, p3c2 r24, q3c2 r23, p4'c2 r22, q4'c2 r21 — exactly the target's case 1/2 registers, with no numbering trick.
- **APPLIED (src/lib/mpv_mcy.c V2, harness U3 = U2 with unused locals dropped, 139w, cases 13/39/30/58 lines):** cases 1-3 load offsets 4/8/12 into
  w1,a1,w2,a2,w3,a3 and pack into the SAME variables (`w1 = (w1 << 24) | (w2 >> 8)`; the frontend numbers pack @N, load @N+1 per case: pack pops first);
  the fifth pair is packed in place on the fifth loads in ALL cases (case 3 `p4 = (w3 << 24) | (p4 >> 8)` — w3/a3's loads p3/q3 must pop before W3/A3
  = r24/r23 before r22/r21; with `w3 = (w3 << 24) | (p4 >> 8)` they were L3 (their w3/a3 packs are removed after them in scan 2: 1-2 neighbours
  short)); case 1 loads `q4 = s1[16]` before `p4 = s0[16]` (group q4 < p4). Steps: S1 case 3 only 172w, S3 + case 2 161w, S4 + case 1 159w, U1
  fifth-pair in place 167w (+4 bytes), U2 + q4-before-p4 139w. Case 3's structural colours are all right now except p0/q0 (below); valid.py 0 conflicts
  everywhere; gsearch2 reaches 180/182 (= all, the two coalesced fifth-pair `mr`s of case 2 are unscored) on U2's graph, so the graph admits the target.
- **sigmatch's case-3 role map swaps p/q for the second half (p3/q3, W2/A2, W3/A3, p4/q4): symmetric roles, scores unaffected; the true target case 3 is
  p3 r24, q3 r23, W2 r28, A2 r26, W3 r22, A3 r21, p4 r18, q4 r19, q0 r17, p0 r8.** Load STATEMENT order in a case changes nothing (T2/T3 = S4); p0/q0
  substituted (steps at the end) 209w.
- **Left (139w), all read in the model:** (a) case 1's p1/q1/p2 webs (target r8/r30/r28, L2) fall to L1 (r17/r18/r19): scan 1 removes case 1's own
  locals q0/p4/q4 (degree < 29, lower vids) before them and takes them from 31 to 26-28 — case 2's identical webs stay at 31 (no own locals there);
  the target's case-1 fifth loads are the same node as their in-place pack (`lbz r22; rlwimi r22`) = webs, not own locals; (b) p0's case-2/3 webs
  are L3 and now pop before the pointers (p0 r0, stride r3, masks r7/r8; the pointers fell from spill picks to L3 on this graph) — the target has the
  masks r3/r7 coloured before p0 (r8) and i (r9); (c) q0's case-3 web is L2 (pop 61, the fresh r20) while the target's q0 (r17) is the last hand-out
  after the case-3 temps hand out r20/r19 and the x's r18/r17 — in the model the tail works when q0c3 pops in L1 before p4c3/q4c3 (they interfere
  with it: r17 covered -> r18, r19). Case 0 lost 8 lines (13) only through a3 (r20 for r24: the r20 hand-out).
- **db_cam, the sched2 dependence sets read off the LOG_LINKS (why no store can break the tie):** every store of the block is REG_DEP_ANTI on EVERY
  earlier load (`stw r30,0x118(r7)` 470 on 433/437/450/462/465/468; the target-copy stores 499-501 on 433..497) and REG_DEP_OUTPUT on every earlier
  store: the memcpy stores are `mem/f` (MEM_SCALAR_P) through a different hard register than the `mem/s` loads, so `memrefs_conflict_p` says 1 for
  every pair and the struct/scalar exclusion of `anti_dependence` needs the WRITE to be the non-struct varying one — it never fires here. The y/z
  dependents therefore differ only in the register edges (y: 471 T, 497 OUTPUT r0; z: 472 T, 491 OUTPUT r8), and the priorities are both
  27 (own store) + 2 (lwz latency). Anti edges cost 0, output edges >= 1, so the stores' 27 comes from the output chain x-store -> target stores.
  The y-first-sched1 alternative is self-contradictory: y r0 needs y's store >= 2 insns BEFORE z's store in the sched1 stream (pri 2/len, tie
  -> lower qty = y), but sched2's store tie (27/27, 18/18 dependents) is LUID = the sched1 order, and the target stores z before y. So the
  target's stream is ours (z-first sched1) with a sched2 difference that leaves no byte and no C-visible RTL difference — the vendor's copy
  had a different RTL form for the same bytes (a different destination-pointer form is the only free parameter: `(plus r10 4)` stores).
- **lib/adx_tsvr `adxt_nlp_trap_entry` 2w (unchanged; the ties-1 "excluded from r0" reading CONFIRMED as the mechanism, the C shape not found;
  /tmp/ties2/tsvr, run.sh + ra.py dumps):** in the RA graph the r0 exclusion is a physical-r0 neighbour edge: it is present on every web that is
  live across a call (r37-r39, r36 ofst1, r33 ofst2v, r53 n1) and on every web used in an rA position (`addi r5,r44,-1`: r44; the `lwz r12,rX,0x18`
  bases r45/r47/r49), absent on `add`/`subf` operands (r51 `add r3,r51,r36`) and on the lha temp r56 (neighbours r1 r3 r33 r36 r37 r38 r39 r53 r54,
  lowest free r0). Probe b3 `register Sint32 t = ofst; asm { addi t, t, 4 } ofst1 += t - 4;` gives the temp `r0` in its neighbour list and
  `lha r4,0xa(r1)` = the target's colour with nothing else moving (12w only from the surviving addi/subi). So the vendor's temp web carried an
  rA-position use that left no instruction. Every codeless spelling of such a use dies BEFORE the graph: a dead asm `addi t2,t,4` / `lwz t2,0(t)`
  (b1/b2) is deleted at backend pass 04 (add-propagation's dead-code sweep); `asm { addi t,t,0 }` (a1) is constant-propagated to `mr t,t` at an
  asm-triggered `constant-propagation` pass (the asm/register form runs 4 extra backend passes: constant-propagation, load-deletion, copy-prop,
  add-prop; the plain C pipeline has 13) and the self-copy is deleted at the RA without the edge; `asm { mr t, t }` (a2) 2w; pointer forms put
  the temp in rB, not rA: `((Sint16*)((Uint8*)&ofst2 + t))[0]` (f3) = `lhax r26,r4,r0` with `addi r4,r1,8` as the base (13w); `(Sint8*)ofst1 + ofst`
  (a3), `(ofst + 4) - 4` (e1), `t2 = ofst + 4; ofst1 = ofst1 + t2 - 4` (e2: frontend reassociates to `add r27,r0,r27` — the temp in rA of `add`
  has NO r0 edge, so `add`'s rA is not a base position), `t2 - t2` (f1), `Sint16 *po = &ofst; ofst1 += po[0]` (f2: add-propagation folds the
  addi into the lha, the edge lands on po's web, not the temp's): all 2w. The post-RA peephole of this function only merges `mr r28,r3; cmpi` ->
  `mr.` and re-bases three loads on r3; the RA deletes three coalesced `mr`s. Left 2w; the C construct that gives a load temp an rA use deleted
  at/after the RA graph (a coalesced copy or a post-RA fold whose rA is the temp) was not found. Catalogue note (MWCC row 1/"swapped registers"):
  "lowest free of r0,r3..r12" excludes r0 for a web with an rA-position use (addi/load/store base) or live across a call — read the neighbour
  list for a physical r0 before modelling a colour.
- Flags: none changed (game/db_cam 12/13, lib/adx_tsvr 5/6). No tree file edited except this section. Nothing built under the lock.

### CRI SWAR kernels pass 21, part 2 (continuation of "CRI SWAR kernels pass 21" above — another agent's section landed between; mpv_mcy 16x16 V2 139 -> 131w APPLIED (case 3's p0 a substituted load); the case-1 residue read: own locals removed in scan 1 pull the p1/q1/p2 webs into L1; not flipped; 2026-09-12)
- **139 -> 131w APPLIED (U4): in case 3 only q0 is kept as a variable (`q0 = s1[0]; s1 += stride;`) and `s0 += stride` moves to the loop end, so p0
  (single use) is substituted into W0 = a backend temp (r8 in ours as in the target, the L3 node gone); case 3 58 -> 43 lines.** The same in case 2 is
  worse (U7 133w). Negatives: the fifth loads substituted in case 3 (U5/U6 203-207w: p4 r18/q4 r19 are callee-saved webs in the target, a temp would
  pop before the x's); the fifth loads written into p0/q0 (U8/U9 240w, size -8: the substituted byte load makes the peephole fuse the byte's
  zero-extension and keep `slwi w3` — the byte must be a VARIABLE so that its copy is the rlwimi base); case-1 load statement order (V1/V3 = U4);
  p0/q0 loaded first with the steps at the end (V2 142w); case bodies swapped in the source (W1 307w: MWCC lays switch cases out in SOURCE order,
  so the vendor's order is 0,1,2,3).
- **The residue's mechanism (131w), read in the model (score.py/why.py; U4 dump): case 1's p1/q1/p2 webs (target r8/r30/r28) are removed in scan 1
  (L1) because case 1's own locals q0 (deg 28), p4 (25), q4 (26) have lower vids and degree < 29, and each removal takes one off p1c1/q1c1/p2c1
  (31 -> 28); case 2's identical webs stay at 31 (no own locals) and colour right. One extra neighbour on q0 alone (score.py edge=q0:@206) makes
  case 1 38 -> 43/47 but pushes p0 to L2 (r31). The target's structure per the model: p0/q0 own locals in L1 (the last two hand-outs r17/r18,
  p0 declared before q0), the case-1 fifth loads ONE node with their in-place pack (`lbz r22; rlwimi r22` = an L2 web taking the latest hand-out
  r22 right after W3c3's), so case 1 has exactly two own locals and p1c1 keeps 29. A web needs a first definition before case 1 that is not dead
  (case 0 has no fifth word; a pre-loop `p4 = 0` is deleted): not found this pass. Also open: q0's case-3 web is L3 (pop 1, r0; target r17 =
  popped in L1 after x0c3 hands out r17) and drags the pointers down a level (stride r3, masks r7/r8); the x's numbering x0 above x1 in each
  case means x0c3 pops before x1c3, while the target's r18 (x1c3) hand-out precedes r17 (x0c3) — in the model the tail closes only when q0c3,
  p4c3, q4c3 interfere (ours: yes) and pop in that order in L1.
- Tools worth keeping in mind (harness): `score.py RA SBS RANGES edge=A:B del=X move=N:pos` = whole-function score after graph edits with the
  structural nodes whose colour/level changed; `why.py RA SOLUTION nodes` = the used/handed sets at a node's pop and which neighbour covers which
  callee-saved register; `mkcase.py` = one-line case rewrites. gsearch2's 180/182 solutions are free vid permutations (own locals above webs etc.)
  and are NOT realisable as written; use them only through why.py to read which neighbour must cover which register.
- Tree: src/lib/mpv_mcy.c V2 (U4 form + comment); mpv_mcy 3/5 (4p 30w, not touched: V2 did not close, its Uint8-pair/backend-CSE question stands);
  objects.py untouched, no flip, only mpv_mcy.o built under the lock. ~/.cache/cri_swar20 DELETED; ~/.cache/cri_swar21 kept small (scripts, bodies/,
  NOTES.md; dumps/objects deleted) for the next V2 pass — delete it there. Kit untouched.

### CRI pass 72 (adx_dcd5 Ste4AsSte 25 -> 11w APPLIED pure C + the existing M1 pin: table indexed directly + `scl` read through its own address; Ste4AsMono 26w unchanged; the codeless multi-def spellings enumerated (copy propagation is reaching-definition based); the stack-parameter round-trip cast keeps the widening `extsh` as a B1 consumer; both targets modelled as kept c1/c2 parameter copies (20/20, 16/18); not flipped; 2026-09-12)
Harness ~/.cache/cri72/ (`mk.py NAME [--ste|--mono] [--src F] 'OLD=>NEW'..` = base.c with exact-string edits inside ONE function; `t.sh NAME [--ra FUNC]` = variant.sh words + ra.py dump `ra_NAME`; `wi.py RA_DIR [--k N] [--ste|--mono] [--all] edge=A:B unedge=A:B ghost=P ghostlike=VID:P fixed=VID:N vid=OLD:KEY del=VID` = chaitin what-if scored against the role maps by name (QTBL/QLIS/MAGIC/C1E/C2E and the sadd/smul/scl load-vs-widening nodes found by pcode pattern), `WATCH=vids` env = per-scan degrees + picks; deleted at the end). The APPLIED bullet and the shared-structure reading of this pass sit BELOW the "Espgen pass 16" section (another agent's section landed between).
- **Backend copy propagation is reaching-definition based, not "single-def only" (pass 71's reading corrected):** an own-local self-copy `asm { mr qtbl, qtbl }` (s10) or a copy chain through a physical register `asm { mr r6, qtbl; mr qtbl, r6 }` (s15) is folded at backend-02 (the first def is dead, the chain is followed); a copy survives only when a use has TWO reaching defs of the destination (a loop-carried second def: s5/s6/s17 keep `mr r23,r0` = +4 bytes, because r76 is live around the loop) or when the source is multi-def at the time of the pass. `qtbl` spellings, all negative (Ste, words): `if (0) qtbl = AdxQtbl` 25 (frontend deletes); `qtbl = nblk ? AdxQtbl : AdxQtbl` 83, size equal (the compare survives as the record form `srawi.` and the coalesced `?:` ghost pair lifts qtbl to r31); `qtbl = AdxQtbl; asm { } qtbl = AdxQtbl;` 25 (an empty asm is not a barrier; the first def is dead); `for (i = 0, qtbl = AdxQtbl; ..)` alone 26 (position) / with the pre-loop def 25 (dead first def); def in the frame loop before the inner loop 25 (hoisted, pre-loop def dead); def between the two `qtbl[]` reads 176 +8; def at the frame-loop end 170 +8; two-way neighbour pins on qtbl in B1 (`r11`/`r12`/`r6` 111/86/32w) or at the frame-loop end (r11/r12/r6/r0: 104/107/148/167w, all +4: the surviving `mr r50,r76` is NOT coalesced — an own-local destination never coalesces (SWAR pass 16 rule confirmed; SetHdr's coalesced `mr r46,r50; rlwimi r46` is a copy into a backend temp)). So Ste's requirement 3 (an own-local leader with fewer B1 neighbours) is unreachable by a surviving copy; the model (pinned tree graph, K=28) shows QTBL r76 must lose ~6 alive neighbours at the L7 scan (34 -> <=27) or the node must have a vid below sc_r (51) — its fixed neighbours are r0 (lwzx base), r1/r72 (arg base), r3-r10, the five parameter ghosts.
- **NEW LEVER (pure C): `sadd = (Sint16)(Sint32)sadd;` (also `sadd = -(-sadd);`) after `nblk = nfrm / 2`** makes the stack parameter's vreg two-def until backend-11: the frontend keeps the narrowing (`extsh r42,r42` at backend-00), backend-09 constant propagation folds THAT into `mr r42,r42` (deleted at 11) but, seeing r42 two-def, does NOT rewrite the hoisted widening `extsh r67,r42` (@140) into `mr` — so the widening reaches the scheduler and the RA as a real `extsh`: the `lha sadd` gets a frees-1 consumer (h3: issued c1, ahead of scl/smul), the node is the @temp r67 (family (B)'s vid, born after the loads: not adjacent to r1/r73), r42 is a short-lived B1 node, and the post-RA peephole merges `lha rX; extsh rY,rX` into `lha rY` (no code). Mono plain form (m0, 178w, size -4) -> 63w, size equal (m8/m25/m29). `asm { extsh sadd, sadd }` / `asm { addi sadd, sadd, 0 }` on a `register` parameter do the same (63w); `asm { mr sadd, sadd }` is folded at 02 (178w); `|= 0`, `^= 0`, `+= 0`, `<<= 0`, `*= 1`, `-= 0`, `/= 1`, `>>= 0` are folded by the frontend (178w); `sadd ? sadd : 0` 150w +20 bytes; `& 0xFFFF` 148w +8. The same round trip on the pointer `scl` (`(Sint16 *)(Uint8 *)`, `&scl[0]`, `(Uint32)`, `+ 0`, `(void *)`) is deleted by the frontend (no narrowing to keep). Model on the m8 dump (K=28, r6 pin after the loop): 8/18 — the picks are nblk then @140 (sadd) because @140 and smul tie at cost 33 / degree 40 at the stuck point and the tie goes to the higher vid; `fixed=41:1` (ONE more never-removed neighbour on smul only, e.g. the r1/r73 arg-base pair that smul loses because the promoted sadd load is no longer the base's last use) -> **16/18 = the target except the q_l/q_r tail**. Both widenings kept (`smul = (Sint16)(Sint32)smul` too, m13/m50): 140w — smul's node becomes @139 (vid 68 > 67: picked on the tie, right) but scl drops into the own-local level (its load is now the base's last use: `fixed=40:2` -> 16/18). So the target's structure = the stack loads issued in PARAMETER order (scl, smul, sadd: the final code's order) with sadd's widening a real consumer — i.e. frees-1 consumers on all three loads (pass 71's config (D)) or something else that keeps the sadd load from being promoted. Dead physical-write consumers `asm { mr r12, smul }` (+ scl, + nblk) 81-123w; two-way neighbour pins on smul/nblk/l1 windows 81-90w (a two-way pin on a PARAMETER coalesces it into the physical register).

### CRI SWAR kernels pass 22 (mpv_mcy 16x16 V2 131 -> 52 -> 22 -> 0w IDENTICAL pure C APPLIED: the offset-0 pair in the same-variable form in every case, no p0/q0 at all; 4p 30 -> 26w APPLIED (dead pixel initialisers); mpv_mcy 4/5, not flipped; 2026-09-12)
Harness ~/.cache/cri_swar22/ (swar21's scripts with paths fixed + `weblist.py RA_DIR` = one line per frontend-02 statement with the assigned
object (`@N` or the variable) and a load/pack signature = the web numbering per case read in seconds; `whyb.py RA SBS RANGES NODES [edits]` =
why.py on the BASE graph (no gsearch solution needed): pop index, level, used/handed sets, which neighbour covers which callee-saved register;
`score.py` gained `coal=KEEP:GONE`, `unedge=A:B`, `show=NODES`; `mkbatch1.py`, `run4p.sh`; bodies/, NOTES.md; ~/.cache/cri_swar21 DELETED).
- **V2 IDENTICAL (X3, tree): cases 1-3 load the offset-0 words INTO w0/a0 with the pointer step right after (`w0 = ((Uint32 *)s0)[0]; s0 +=
  stride; a0 = ((Uint32 *)s1)[0]; s1 += stride;`) and pack in place (`w0 = (w0 << 8) | (w1 >> 24)`), exactly like the other pairs; the fifth
  pair in place on its own loads with p4 loaded BEFORE q4; case-0 declaration order the natural `w0, a0, w1, a1, w2, a2, w3, a3, p4, q4`.**
  Steps: V03 = `q0 = 0, p0 = 0` dead initialisers + case 3 both p0/q0 kept by mid steps 131 -> 52w (8/29/6/9); W02 = + natural declaration
  order + p4 before q4 52 -> 22w (cases 0/2/3 identical); X3 = + no p0/q0 (w0/a0 same-variable form in all three cases) 0w. X1 (the w0/a0 form in
  case 1 only) 21w: case 2 breaks (its p0/q0 webs lose their group).
- **Mechanisms, read in the model (score.py/whyb.py on the tree's graph) and confirmed by the builds:**
  (1) `q0`'s case-3 web was the spill-level (L3) node that pushed stride/masks off r0/r3/r7: as the LAST-defined variable its group had the
  highest @ = the LOWEST web vid, so it was visited first in every simplification scan while its 32 neighbours (all 8 loads, all 8 pack webs and
  their srwi ghosts, the pointers, i, the masks) were still alive; the target's q0c3 (r17) and p0c3 (r8) are removed in scan 1, which needs the
  node visited AFTER p4c3/q4c3 (removed there) — i.e. the offset-0 variables must be the FIRST-defined ones (groups w0/a0 = the highest web vids).
  (2) Case 1's p1/q1/p2 webs (target r8/r30/r28) fell to L1 because case 1's OWN LOCALS q0/p4/q4 (deg 28/26/27) have lower vids and are removed
  before them in scan 1 (31 -> 28). With the offset-0 loads as webs of w0/a0, case 1's only own locals are p4/q4 (2 removals: 31 -> 29 = L2), and
  p0c1/q0c1 (vids above every other web) are removed in scan 1 (30-2 = 28, 28) and pop in L1 after the x temporaries handed out r17/r18: p0c1
  r17, q0c1 r18 = the target's "last two hand-outs" — they are webs, not own locals (`move=q0:98.5 move=p0:98.6` predicted 43/47 before the build).
  (3) The fifth pair: p4's group before q4's (`p4 = s0[16]; q4 = s1[16]` in case 1) gives P4pack r22 / Q4pack r21 in cases 1-3 AND p4c3 r18 / q4c3
  r19 (L1 pops in group order); pass 21's q4-before-p4 swap had compensated the L3 damage and is wrong once (1) is fixed. Same for the case-0
  `a3` declaration trick: with r20 no longer handed out early, the natural order gives w2 r21, a2 r22, w3 r23, a3 r24.
  (4) **Dead declaration initialisers (`Uint32 q0 = 0, p0 = 0;`) move a variable's web GROUP to the front of the @ numbering (the group order
  follows the first definition INCLUDING deleted dead initialisers) but do NOT change which definition carries the variable's own-local node
  (case 1's load stayed `p0`, not `@N`, in T1/T2: the own-local node is the first SURVIVING definition).** A live prologue definition without
  code does not exist: `p0 = (Uint32)s0 & 3; __dcbt(s1, 0); switch (p0)` is substituted into the switch (arithmetic is substituted across a
  dcbt; only LOADS are blocked by store/if/dcbt/redefinition), `q0 = 0; __dcbt(s1, q0)` and `q0 = p0` are propagated (W03-W07 = base). The
  offset-0 loads have to be webs of a variable whose own-local node is case 0's load: w0/a0.
  (5) Case-3 statement order of the loads (q0 first, p0 first, both kept, steps mid/end: V05-V12) changes nothing or +11w — the pre-RA order is
  DAG/priority driven; only the numbering and the own-local set matter.
- **4p 30 -> 26w APPLIED (P2): `Uint32 a0 = 0, b0 = 0, a1 = 0, b1 = 0, a2 = 0, b2 = 0;`** (the pixel pairs' groups first). Census: a0/b0 only or
  a0..b1 30w, a1/b1 only 84w, a2/b2 only 90w, all six in reverse declaration order 95w, the sums initialised 104w, `Uint8` pairs 106w (= pass
  13), `Uint8` + initialisers 106w. The block-1 cut (target block 1 ends after the 9th sum, ours two statements later) is unchanged — the 26w are
  the cut's schedule plus its register consequences; the Uint8-pair re-model was not done (no 4p role map in sigmatch; 30-minute box spent on
  the initialiser census).
- Catalogue-grade facts for the MWCC table (row "range-split webs of a switch's cases coloured in the wrong order" / row "lowest handed"):
  a variable first defined LAST has the lowest web vids = visited FIRST in every scan = the spill-level candidate of the function; make the
  short-lived per-case loads webs of the FIRST-defined variables (the same-variable form for every pair, case 0's load order = the group order);
  dead declaration initialisers reorder groups without code; an own local of a later case removed in scan 1 takes one off every web live with
  it — count the case's own locals against the webs' 29-threshold before anything else.
- Tree: src/lib/mpv_mcy.c V2 (X3 form + comment), 4p (P2 initialisers + comment); mpv_mcy 4/5 (4p 26w), not flipped, objects.py untouched, only
  mpv_mcy.o built under the lock. ~/.cache/cri_swar21 deleted; ~/.cache/cri_swar22 kept small (scripts, bodies/, NOTES.md; dumps/objects
  deleted) for the 4p pass — delete it there. Kit untouched.
- **APPLIED (src/lib/adx_dcd5.c ADX_DecodeSte4AsSte 25 -> 11w, Mono 26w unchanged, sizes equal, locked ninja + bytecmp 2/4; objects.py untouched):** the table indexed directly (`AdxQtbl[d & 0xF]`, no `qtbl` local: the backend hoists the `lis/addi` pair into the loop-code-motion preheader B51, so the address temp is NOT adjacent to the B1-only values and pops one level after the own locals -> r22 exactly like the original; the own-local pointer form kept the addi temp live from B1 = L8 with the own locals -> r31/r12, s40: 88w), the M1 pin moved to `asm { mr r6, l1 }` after the hist loads (`register l1`, as Mono's), and the key pointer read through its own address (`Sint16 **pp; pp = &scl; key = **pp;` in both scale blocks, `*scl = key` stores unchanged): the address-taken parameter has no web, its loop read is hoisted into B51 as a slot load, and that load keeps the argument base r71 live PAST the smul/sadd loads -> sadd/smul gain the r1/r71 pair (+2) and survive the own-local scan (sadd r0, smul r11). Model: s40 graph + `edge=42:1 edge=42:71` (or `fixed=42:2`, or `ghost=9 ghost=10` = kept c1/c2 parameter copies dying at their extsh) = **20/20**; `fixed=42:1` 6/20 (sadd is 26 vs smul 28 at the own-local scan, K=28: exactly the base pair). Census: `pm = &smul` 24w, `ps = &sadd` 27w, `pp = &scl` with the pin after the loop 21w, pin before the loop 11w (s70 = the tree), first read `*scl` + second `**pp` 11w too. Residue 11w = B1 order (8 words: ours `add r12` (scl's register is free at the add because scl's load is now the hoisted B51 load, issued last; target `add r19` = all three loads before the add), `srawi` before the loads, `lwz r12` last) + 3 words at +0x1d4 (the `c1*t`/`c2*rr1` product pair of `rr1 = q_r*sc_r + ((c1*t + c2*rr1) >> 12)` swapped). So Ste is now in the same state as the tree's Mono: the right colouring reached by a hoisted slot load that cannot sit before the `add`.
- **Both residues now read the same way: the original's B1 loads all three stack parameters as webs in parameter order AND the argument base stays live past the sadd load** (Ste: `edge=42:1 edge=42:71` on the all-web graph = 20/20; Mono: the m8 graph (sadd widening kept) + `fixed=41:1` = smul keeping the base pair = 16/18, q_l/q_r only). No C found for a fourth base use: there is no 12th parameter (the matching caller passes 11), c1/c2 are register parameters (a home-slot access would be a real store), scl/smul/sadd cannot be both a web and address-taken. Negatives: c1/c2 round trips `c1 = (Sint16)(Sint32)c1` (register parameters get `extsh t,r38; mr r38,t` = a new temp for C1E, 60w; `asm { extsh c1, c1 }` is folded through operand renaming into `extsh r38,r9` = no ghost, 99w; `asm { addi c1, c1, 0 }` no effect); dead physical writes `asm { mr r9, l1 }`/`mr r10` behave like hard pins (r9/r10 leave the colour set, frame +0x10, 42-99w); two-way pins on the stack parameter sadd with r14/r17/r18 coalesce the web into the physical (113-115w); dead virtual consumers `asm { extsh t1, smul }` are deleted before scheduling (no effect); Mono with `pp = &scl` on top of the tree form 42w, on the round-trip form 68w.
- **The shared target structure, read in the model (both functions, the r6 pin's K=28 graphs): kept c1/c2 PARAMETER COPIES.** `ghost=9 ghost=10` (two coalesced copies `mr r38,r9` / `mr r39,r10` dying at their widening extsh = +1 on exactly the values live at B1's end: scl, smul, sadd, nblk, l1, l2, rr1, rr2) on the all-web direct-table Ste graph (s40) = **20/20**, and on the plain-web Mono graph (m0: `key = sadd + key * smul`, AdxQtbl direct, pin after the loop) `ghost=9 ghost=10 fixed=42:1` (or `edge=42:1`, or `edge=42:73`) = **16/18** (q_l/q_r only); the m8 (sadd-widening) graph needs `ghost=9 ghost=10 fixed=41:1` instead. The compiler confirms the graph: `asm { mr r3, c1 } asm { mr r5, c2 }` after `nblk = nfrm / 2` (s96) yields exactly the ghosts r38->r9/r39->r10 and the model scores 18/18 on that dump — but the compiled function is 108w because **every asm-written physical register leaves the colour set even when the write is dead** (chaitin `--check` on s96 is exact with `--k 26` = 29 - r6 - r3 - r5; the pass-47 K rule applies to dead writes into already-ghosted registers too), so no asm can supply these ghosts. C spellings that fail to keep the copies: any identity redefinition of a REGISTER parameter (`c1 = (Sint16)(Sint32)c1`, `-(-c1)`, `(Sint16)(c1 + 0 | 0 * 1 ^ 0 >> 0 - 0 / 1 & -1)`: the frontend emits `extsh t,r38; mr r38,t`, so C1E becomes the temp t and the copy is propagated, 60w; `& 0xFFFF`/`(Uint16)` +8 bytes); `asm { extsh c1, c1 }` (the operand is renamed to r9, 99w); `asm { addi c1, c1, 0 }` (folded, 88w). **When IS a parameter copy kept (read off the dumps, single-def parameters):** when the parameter reaches the RETURN VALUE — `return nfrm` keeps `mr r33,r4` (`mr r3,r33`), `return nblk * 2` drops it (s95: `rlwinm r74,r4`, nfrm's ghost gone, 115w); `return (Sint32)histl` / `(Sint32)histr` keeps histl's/histr's copy (s102/s103) while `return nfrm + (Sint32)histl` keeps histl's but drops nfrm's (`add r3,r4,r35`). The after-loop hist STORES do not keep a copy (`sth r56,r6,0` in the unpinned Ste). c1/c2 cannot reach the return without code (`return nfrm + c1 - c1`, `(nfrm | (c1 & 0))` are folded by the frontend: 88w = s40). Left open: the vendor's source shape that keeps the c1/c2 copies (a return-value path, or a use the propagation refuses).
- Ste residue probes on the tree form (11w): `nblk = nfrm / 2` before/after `pp = &scl` or after the hist loads 11w (no change); `i < nfrm / 2` in the loop test 19w. The 3-word tail at +0x1d4 = the `c1*t` / `c2*rr1` product temps of `rr1 = q_r*sc_r + ((c1*t + c2*rr1) >> 12)` with swapped colours (r21/r26): the target pops `c1*t` first (same instruction order = an IR temp-numbering tie, like Mono's q_l/q_r), not attempted.
- Two more facts for the catalogue: (a) the sadd round trip works only for STACK parameters because the frontend emits the narrowing IN PLACE on a load-defined vreg (`extsh r42,r42`), while on a register parameter/local it emits `extsh t,x; mr x,t` (a new temp); (b) dead asm writes into the reserved r2/r13 are emitted (+4 bytes each), so there is no K-free physical register to write. Pre-RA check (schedwhatif on the all-web Ste dump): kept c1/c2 copies would be frees-1 `mr`s taking the c1/c2 second slots and push the stack loads later, so the ghost hypothesis does not by itself explain the target's "loads before the add" B1 order (pass 71's (D)); the two facts about the vendor's B1 stay separate.
- Final state: src/lib/adx_dcd5.c ADX_DecodeSte4AsSte 25 -> 11w APPLIED (Mono 26w unchanged), adx_dcd5 2/4, sizes equal, not flipped, no `ninja -k 0`; objects.py untouched. Harness ~/.cache/cri72 deleted. Next: a C shape whose c1/c2 parameter copies reach the RA (the propagation keeps a copy whose use is a `mr rPHYS, x` — argument/return moves only), or one that loads all three stack parameters as webs before the `nfrm / 2` add with the argument base still live afterwards.

### CRI SWAR kernels pass 23 (mpv_mcy Matching 4 -> 5/5 FLIPPED, 111 OK: MPVMC16_OneRef4p_TuneC 26 -> 0w pure C — the pixel-9 pair loaded BEFORE the d[0]/d[1] stores; the "block 1 ends after the 9th sum" reading of passes 12/13 was wrong, the >100 split after `b2 = s1[11]` is the target's; 2026-09-12)
Harness ~/.cache/cri_swar23/ (deleted; ~/.cache/cri_swar22 deleted too): `gen.py NAME mask=<sums>:<roles>[:form] p9=before|mid` body generator of the
three-pair form, `run.sh NAME` = variant words + ra.py dump + the extra backend passes + `cnt.py RA SRC` (cumulative initial pcode count per source line
= where the >100 split falls), `roles.py RA..` (pixel/sum role -> colour per dump against the target's registers), `valid4p.py RA` (named-node
interferences vs the target colours), `w.py RA keep=X` (chaitin what-if: X survives scan 1), `norm.py` (vid-blind pcode dump diff). Kit untouched.
- **The match (h0, APPLIED): `a0 = s0[9]; b0 = s1[9];` moved from after `d[1] = ..` to before `d[0] = ..`; nothing else.** Mechanism: a load through a
  `Uint8 *` LOCAL written after a store in statement order is in the store's alias class (pass 58: only a `const T *` PARAMETER gets its own object
  record), so the pre-RA DAG has `stw d[1] -> lbz s0[9]` and the scheduler puts the pixel-9 loads right after the second store (positions 74/75 of
  block 1) while the target issues them before `stw d[0]` (target lines 62/66 vs 72); with the loads before the stores in the source they schedule at
  57/61 and the block's live ranges, the Chaitin levels (L3 = the five loop variables alone, L2 = a6 a7 a2 a8 p2 p3 p4 a10 a11 a12 a14 a15 b15 p0'..p5')
  and every colour follow. The pixel-10 loads stay after the stores in the source (target lines 77/78 after `stw d[1]` at 75). Oracle for the
  catalogue: **a load's position relative to a store in the target's block is the source's statement order when the pointer is a plain local (the
  alias edge is one-directional: loads before the store in the source may sink below it, loads after it can never rise above it).**
- **The split reading corrected:** the target's block 1 = 75 final instructions ending with p1''s `add r30, r10, r30` (line 84), i.e. it holds the
  pixel-10/11 loads and the p1' sum; the initial-code count (cnt.py on the tree: d[0] 75, d[1] 83, 9th sum 91, `a2 = s0[11]` 100, `b2 = s1[11]` 101 ->
  B3 = 101 pcodes, split before p2') is the target's. Passes 12/13 read "block 1 ends after the 9th sum" off the post-RA order (the 9th sum's last add
  at position 67 followed by the pixel-10 loads) — that was the schedule, not the block boundary.
- **The later-deleted-pcode levers, measured on this body before the statement move was found (all correct as levers, none needed here):**
  `((Uint32)a & 0xFF)` on a Uint32 pixel variable (twice-used pixel, masked in ONE of its two sums) or `(Uint32)(a & 0xFF)` on a single-use pixel =
  +1 initial pcode (`rlwinm 0,24,31`), turned into `mr` by the added `constant-propagation` pass and deleted by the `copy-propagation` after it, before
  scheduling; the pipeline gains constant-propagation + load-deletion + a second copy-propagation/add-propagation/peephole-forward round and NO
  common-subexpression-elimination (each pixel masked once = no duplicate expression); the pre-RA pcode is identical to the unmasked body modulo vids.
  `(Uint32)(a & 0xFF)` on a twice-used pixel = 0 net (it breaks the frontend's cast CSE: the `(Uint32)a` @temp `mr` disappears, the rlwinm replaces it).
  Ten masks (`mask=0-15:bnew 0:bold`) put the split exactly after the 9th sum: 136w = pass 13's number, without any CSE pass — so pass 13's "the CSE
  pass drops a8/p8 to L1" was a misattribution: with that split the block-1 schedule changes (the 9th-sum chain becomes the block's tail, a8's total
  degree 37 -> 30, a10/a11/a12/p1'/p5'/a14/a15/b15 drop to L1) and the loop variables' degree after scan 1 falls to 28/29 (d 28, stride 28): L3
  collapses into L2 and everything is coloured in vid order from r0. The chaitin what-if (`w.py keep=a8`) restores L3 but not the L1/L2 colours
  (17-18/46): the schedule, not the level set, was the residue.
- **Model facts kept:** valid4p (named nodes vs target colours) shows 0 conflicts in the 26w tree AND in the mis-cut variants — a conflict-free named
  graph does not mean the schedule is the vendor's; the tree's graph scores 45/46 in chaitin.py against the hand-read target table (the miss is a
  table typo), and `chaitin.py --check` is IDENTICAL on every dump of this pass.
- Flip: objects.py `# CRI SWAR pass 23` block (`"lib/mpv_mcy.c": True`), locked `ninja -k 0`, `dtk shasum -c` 111 OK. Tree edit: src/lib/mpv_mcy.c
  4p function (two statements moved + comment). No pins, no asm, no pragmas, no tags anywhere in the unit: 1p/H2/V2/4p pure C.

### CRI pass 73 (adx_dcd5 Ste4AsSte 11 -> 3w and Ste4AsMono 26 -> 5w APPLIED: the c1/c2 parameter copies are kept by DEAD asm writes into the ALREADY-pinned r6 (`asm { mr r6, c1 } asm { mr r6, c2 }`, K unchanged, size equal), the table value written back into the nibble (`d = AdxQtbl[d & 0xF]`: a range-split web coloured before the mix); the round-trip/`ps`/`**pp` grid is all negative; not flipped; 2026-09-12)
Harness ~/.cache/cri73/ (`mk.py NAME [--ste|--mono] [--src F] 'OLD=>NEW'.. ['^ANCHOR=>LINE' inserts after, '@OLD=>NEW' edits the signature]`, `t.sh NAME [--ra F] [--sd F]`, `gh.sh NAME F` = words + ghosts + kept B1 `mr`s, `combos.py ste|mono` = the lever grid, `wi.py RA_DIR [--k N] ref=VID:rN ghost=P fixed=VID:N edge=A:B vid=V:KEY` = chaitin what-if scored against the dump's own colours with corrections; deleted at the end).
- **Task A (lever grid, 32 Mono + 32 Ste variants: round trip on sadd/smul/both x `ps = &sadd` x `pp = &scl` x pin before/after the loop): nothing below the tree's 11w/26w.** Mono: `ps` + any round trip = +4 bytes (the round trip on an address-taken parameter is a real load/extsh/store), `pp` on Mono 30-216w, round trips without `ps` 63w (sadd or smul alone), 140w (both) as pass 72. Ste: every round trip is +4 bytes (`lha r21; .. extsh r0,r21` — the peephole merges `lha; extsh` only when the pair is adjacent after the post-RA schedule, which Mono's B1 gives and Ste's does not), `pp`+pin-after-loop 21w, `nopp` 88w, `ps` on Ste 15-27w.
- **Task B FOUND: `asm { mr r6, c1 } asm { mr r6, c2 }` after `nblk = nfrm / 2` (c1/c2 declared `register` in the definition) keeps both parameter copies (`mr r38,r9` / `mr r39,r10` survive to the RA, coalesced -> ghosts r38->r9, r39->r10 dying at their widening extsh) and costs NOTHING: r6 is already out of the colour set (the M1 pin `asm { mr r6, l1 }`), so K stays 28, and the dead writes are deleted (size equal).** Pass 72's `mr r3, c1`/`mr r5, c2` lowered K because r3/r5 were new pinned registers; the rule is "a physical register named in ANY asm write leaves the colour set" — naming the SAME register twice is free. With the tree's `**pp` form: Ste 11 -> 6w (B1 order: `lha smul; lha sadd; srawi; lwz scl` — scl's hoisted slot load still last); with `key = *scl` (all three stack parameters as webs, no `pp`): **Ste 3w** (b5), size equal, the B1 block IDENTICAL (add r19, lis, lwz r12, lha r11, srawi, lha r0, addi). One dead write alone (c1 or c2) = 88w (sadd needs +2, as pass 72's model said). The residue 3w = the `c1*t`/`c2*rr1` product temps of `rr1 = q_r*sc_r + ((c1*t + c2*rr1) >> 12)` (target r21/r26, ours r26/r21): model (wi.py on the b5 dump, K=28) = any ONE more never-removed neighbour on the c1*t temp (r121, degree 27 = removed in scan 1 before r122) or a vid above r122's, or +1 on both (a value live across the two products: only rr2's register r30 is free in B26 in the target, so a real value there is impossible; the target's pre-RA B26 must have had a pcode between/around the products that vanished — a coalesced copy — or the products numbered c2 first).
- **Mono: the same dead writes 26 -> 22w (d1: the two `lis` pairs now colour like the original's; one write alone 22w too; writes placed inside or after the loop 115w = a whole-loop ghost, +1 on everything), then the q_l/mix tail read in the model (wi.py on the d1 dump, refs from the fdiff): q_l must be a node with a vid ABOVE the mix web @149 (any vid in 59.5..100 = 107/110; as an own local it is removed before the web in the same scan and pops after it). A web of a variable FIRST DEFINED BEFORE `t` in the loop gets that vid: `d = AdxQtbl[d & 0xF]; ... l1 = d * sc_l + ..` (g3; `s`/`key` reused the same way: g1/g2, all 5w; `dr = AdxQtbl[dr & 0xF]` for the right channel too: g4 5w = APPLIED; a separate mix local `m` 252w). On Ste the d/dr form is neutral (3w with both, 10w with dr only) and applied for the same source shape.**
- **APPLIED (src/lib/adx_dcd5.c, both functions; locked ninja + bytecmp: Ste 3w, Mono 5w, sizes equal, .rodata OK; objects.py untouched, not flipped, no `ninja -k 0`):** `register Sint16 c1, register Sint16 c2` in both definitions (the asm needs register operands; the prototypes unchanged), `asm { mr r6, c1 } asm { mr r6, c2 }` after `nblk = nfrm / 2` (tag M1, kept parameter copies), Ste back to `key = *scl` (no `pp`: all three stack parameters are webs, the B1 block is now IDENTICAL: `add r19; lis; lwz r12; lha r11; srawi; lha r0; addi r22`), `q_l`/`q_r` removed in both, `d = AdxQtbl[d & 0xF]` / `dr = AdxQtbl[dr & 0xF]` before the stores and `l1 = d * sc_l + ..` / `rr1 = dr * sc_r + ..`. Mono keeps `ps = &sadd` and the pin after the loop.
- **Residues, read in the model (both would close with ONE never-removed neighbour on one temp):** Ste 3w at +0x1d4 = `rr1 = dr*sc_r + ((c1*t + c2*rr1) >> 12)`: the `c1*t` temp (27 neighbours, K=28) is removed in scan 1 before the `c2*rr1` temp and pops after it (r26/r21; target r21/r26); `fixed=c1t:1`, `vid=c1t:above c2rr1` or +1 on both = 92/92. A real +1 is impossible (every register but rr2's r30 is live in that block in the target and the block's final code has nothing between the products); swapping the operands (`c2*rr1 + c1*t`, j1) swaps the instruction order too (the pre-RA tie is input order), so the original's expression order is c1 first and its `c1*t` temp had one more neighbour or the higher vid. Mono 5w = (a) the `nfrm/2` add temp r0 vs r12 (2 words: the `ps` form's hoisted sadd load is B1's last load, pass 71; the plain-web form with both round trips (i4) colours everything right but reorders B1: 13w) and (b) the l1 statement's `(c1*l2 + c2*l1) >> 12` shift temp (r130, 27 nbrs) must pop BEFORE the `d*sc_l` product (r126, 28 nbrs, L2): `fixed=130:1` = 109/110; a real value live at the srawi but not at the product cannot exist (the product spans the interval), so the original's srawi temp is a coalesced leader or has a ghost neighbour — not spelled. Probes without effect: `rr2 = t` moved to the body end (i5), the merged `t` statement (i6), `((..) >> 12) + d*sc_l` (j2/j4), the shift into `s` (j5), `d = d * sc_l` (j6).
- Catalogue rows for the MWCC table: (1) "the target keeps a single-def PARAMETER copy (a ghost dying at its first use) that ours propagates away" -> `asm { mr rP, param }` with rP a register that is ALREADY named in another asm pin of the function (K unchanged, the write is dead and deleted; a NEW register would leave the colour set, pass 72); the propagation keeps a copy whose use is a write to a physical register. (2) "a table value / block local must be coloured before a range-split web of a later-defined variable" -> write it into a variable first defined EARLIER in the loop body (the nibble `d`): its web is numbered before the later variable's webs (higher vid), while an own local declared anywhere is removed before every web.
- Harness ~/.cache/cri73 deleted; kit untouched. Tree: src/lib/adx_dcd5.c both functions + comments; adx_dcd5 2/4 (3w + 5w), objects.py untouched.

### CRI pass 74 (adx_tsvr `adxt_nlp_trap_entry` 2w, `lha r4` vs `r0` in B16: the five "today's levers" hypotheses (H1-H5) modelled and probed; IN PROGRESS; 2026-09-12)
Harness ~/.cache/cri74/ (`mk.py NAME 'OLD=>NEW'..` = base.c with exact-string edits inside the function; `t.sh NAME [--sbs] [--ra]` = variant.sh words + ra.py dump `ra_NAME` + `wi.py` colour diff; `wi.py RA_DIR [--base RA_DIR] edge=A:B ghost=VID:PHYS vid=VID:POS del=VID` = chaitin what-if printing the `lha rX,r1,0(ofst)` temps' colours and every node whose colour moved; deleted at the end).
- Model on the tree dump (chaitin.py `--check` IDENTICAL): `edge=56:67` or `ghost=56:0` -> r56 r4, nothing else moves (ties 2 confirmed). `vid=56:0` / `vid=56:33` (the temp ranked as the lowest own local or anywhere else) -> still r0: NO vid order gives r4, because nothing live in B16 is r0-coloured — H3 (two-use own local) is dead in the model before writing C. Every node live in B16 of the target is accounted for by an instruction (r27 ofst1, r28 n1, r3 n2, r26 ofst2v's def, r29-r31), so an r0-coloured L1 neighbour would need an invisible instruction: impossible; the vendor's temp had a PHYSICAL r0 edge (an addi-family rA or a memory base use that left no instruction).
- H1 (round trip on the lha'd Sint16): `ofst` is an address-taken stack local, and `ofst = (Sint16)(Sint32)ofst;` / `ofst = -(-ofst);` / `ofst1 += (Sint16)(Sint32)ofst;` are deleted by the frontend entirely (2w, no store); through a register copy `Sint16 o; o = ofst; o = (Sint16)(Sint32)o; ofst1 += o;` (also `register`, also `-(-o)`) the frontend folds `o` away and emits `lha r56; extsh r57,r56` — the extsh triggers the 17-pass (constant-propagation) pipeline, which folds it into `mr` at pass 06: 2w, unchanged. `ofst1 -= -ofst` 12w (`neg` kept). `(Sint16)*(Uint16 *)&ofst` 12w: `lhz r0; extsh r0,r0` — the post-RA `lha;extsh` merge does NOT apply to lhz, and a load temp and its widening never interfere (the load dies at the extsh), so the pass-72 widening node can never be the r0 neighbour of the load temp here.
- H2 (a new r0 node from a round trip on ofst1/n1): both are Sint32 — no narrowing to keep; and a real node would be an instruction. Not written.
- H4 (inlined helper `@ret`): ties 1 t1-t5 already showed the helper local coalesces INTO the lha temp (2w); an `@ret` of an inlined helper is a vreg, not physical r3 — the "mr rPHYS" copy of pass 72 exists only for a real call. Not repeated.
- H5 (statement order vs a store): B16 has no store; `ofst2v = ofst2` first was 2w in pass 63. Not repeated.
- New facts about what reaches the RA (read off tools/research/mwccdbg/out dumps): the RA deletes, besides coalesced `mr`s, DEAD DEFS that peephole-forward's forward substitution left behind — sfmpv_ChkBufSiz `extsh r147,r46; sth r147` -> pass 01 rewrites the store to `sth r46` and the dead `extsh` survives passes 02-17 (degree 0) to be deleted by the RA; ADXB_DecodeHeaderAdx 1 extsb, cvFsAddDev 1 li, sfcre_AnalyMpv 7 rlwinm likewise. So a dead def is NOT generally swept before the RA (the pass-04 sweep that removed ties-2's dead asm `addi`/`lwz` is add-propagation's own). A dead def's SOURCE operands stay in the liveness/interference computation. `addic.` (`addicr` pcode: `t + 4 == 0` -> p1, 14w) marks its rA no-r0 exactly like `addi` (t r33 had physical r0 in its list and coloured r4).
- Zero-opaque-to-the-frontend probes (aiming at `li z,0; add Y,t,z` -> peephole-forward `addi Y,t,0`, a coalescable copy with t in rA): the frontend's constant propagation is reaching-definition based and runs after dead-store deletion — `z = 0` in an arm + `z = 0` at the join (y1/y2), `z = 0` at the join + `z = n1` in a return arm (y4), `z = 0` alone (y3): all folded to `ofst1 += ofst` (no `[z]` in frontend-01), 2w. `(Uint8 *)ofst + 4 - 4` across statements (z1), `x += 4; x -= 4` (z3), `y = x + 4; x = y - 4` (z4): folded by the frontend too.
- **The inlined-helper constant (`z = helper()` with `return 0;`) is folded by the frontend in EVERY form tried here** — trivial body (w1-w3), a body with the n2 if/else writing `*pn2` (w8), a body with a `do {} while (0)` (w9), a real loop (w11/w12: the loop is emitted, `i < 0` is not folded, and z is still substituted), a side-effecting body `*pv = *po` (w4/w7), two uses of z (v1-v5: `ofst2v += z`, `ofst1 += z`): no `li z,0` ever reaches the backend (mps_lib's MPS_Init `ret` survived because it is compared AND returned; not reproducible with an add operand). So a `li 0; add Y,t,z` -> peephole-forward `addi Y,t,0` (the one plain-C route to a coalescable copy with t in rA) is unreachable.
- **`addis` marks its rA no-r0 like `addi`, and the frontend keeps `(Sint16)(ofst + 0x10000)`:** k5 = `ofst1 += (Sint16)(ofst + 0x10000);` -> `lha r4; addis r0,r4,1; extsh r0,r0; add r27,r27,r0` (12w, +8 bytes): the lha temp r56 gets physical r0 in its list and colours r4 = the target's colour, with the two extra instructions. The extended pipeline's constant propagation (forced by a `Sint16 o; o = ofst; o = (Sint16)(Sint32)o;` pair, k10 — that pair is what triggers the 17-pass pipeline, not `register`/asm alone: k5-k8 ran the 13-pass one) does not fold `extsh` of an `addis` result. `(Sint32)ofst + 0x10000 - 0x10000` (k4) is folded by the frontend (2w).
- Asm rA forms on a `register` copy `t = ofst`: `asm { addis t2, t, 0 }` deleted (2w, no edge kept); `addic`/`addic.`/`addze`/`addme`/`subfic` `t2, t, 0` are XER writers and stay as real instructions (12w) AND asm-generated pcodes carry no rA mark (`addic r4, r0, 0` was emitted with r0 in rA) — asm cannot fake the frontend's mark either. `?:` with identical arms `(n1 != 0) ? ofst : ofst` is NOT folded (17w: a compare + two arms). `(Sint32)ofst + ofst1` 2w (`add r27,r0,r27`, no rA mark on add), `*(Sint16 *)&ofst` 2w.
- **Conclusion for the record:** the vendor's `lha r4` value carried an rA-position use (addi/addis/memory base) that left no instruction. The only instruction classes the RA itself deletes are coalesced `mr`s and dead defs; every dead def with an rA-position operand that plain C can produce (`addi`, loads, stores) is swept by add-propagation at pass 04, and `addis`/`addic` results the frontend keeps are real code. The remaining unexplored construct is a load/addi through t whose result dies at pass 05 (copy-propagation deleting a dead `mr` whose source is that def) — no C spelling was found that leaves such an `mr` for the backend (the frontend removes dead copies and constant locals by reaching-definition substitution first). adx_tsvr stays 5/6 (2w); no tree edit; nothing built under the lock; ~/.cache/cri74 deleted; kit untouched.

### CRI pass 75 (sfh_main `SFH_AnlyElemSmpHz` 6w, M4 `stwbrx` fold: H1-H4 modelled and probed, all negative; the post-RA fold rule read off the GC/2.6 binary (routine 0x503180, registered unconditionally for STW/STWX) — its gates are all met by the target's own instruction stream, so no C spelling of the vendor's chain can escape it in this build; nothing applied, nothing flipped; 2026-09-12)
Harness ~/.cache/cri75 (t.py NAME 'OLD=>NEW'.. = exact-string edits of the SmpHz body (`@` prefix = whole file) through variant.sh, one line + the `*` rows; wi.py = chaitin.py what-if with `edge=A:B` / `mv=A:AFTER` / `ghost=VID:PHYS`; pe.py = the PCode opcode table of GC/2.6 mwcceppc.exe by VA (STW 0x31, STWX 0x33, STWBRX 0x35, RLWINM 0x67, RLWIMI 0x69); ra_*/sd_* dumps; deleted at the end). Tree untouched (no source edit, objects.py untouched, no ninja).
- **State: 35/36, 6w (`lwz r6` vs `r4` + the target's `rlwinm r0,r6,8,8,15; rlwimi r0,r6,24,0,7; rlwimi 24,16,23; rlwimi 8,24,31; stw r0,0(r5)` vs ours `stwbrx r4,r0,r5`; size 0x15c/0x14c).** chaitin.py IDENTICAL on the base dump; the word r36=@894 has neighbours r3 (the `li r3,1` @ret), r5 (val), r55-r58 (the acc chain); r4 is free because `id` (0/0, propagated into physical r4) dies at `clrlwi r0,r4,24`. `edge=36:4` -> r6 with nothing else moving (pass 65 confirmed).
- **H1 (`id` live at the load / load before `id`'s last use): negative.** The load hoisted above the type test as a `Uint32 w` / `register Uint32 w` statement (h1a/h1b) moves the lwz into the diamond's predecessor (17w); `id = (Uint8)(Uint32)id` after or before the store (h1c) is deleted by the frontend (6w); `sfh_GetStmType(Uint32 id)` (h1f, the copy-parameter form) 6w, other functions unchanged. No pure-C reader of `id` after the type test survives the frontend without code; the target's B37 has no instruction of its own that could be that reader, so the r4 blocker is not `id`.
- **H2 (`(Uint16)` breaker without the mask node): negative, and the mask is structural.** `SWAP32((Uint16)x)` (h2a), `Uint16 h = x; SWAP32(h)` (h2c), `(Uint32)(Uint16)x` (h2d): chain kept, 5w, `clrlwi r4,r0,16` + the base term `rlwinm r0,r0,8,8,15` reads the UNMASKED load while the three rlwimi read the masked r4 (the frontend drops the cast only from the term whose mask makes it redundant; the fused rlwimi operands keep it) — two source registers is exactly what stops the fold. `(Uint16)` on the RESULT (h2b): chain kept + `clrlwi r0,r0,16` between the last rlwimi and the stw (6w, +4). `(Sint16)` (h2e/h2g) = `extsh r4,r0` then `stwbrx r4` (all four terms read the extsh: fold). `(Uint8)` (h2f) 5w, two terms vanish.
- **H3 (round trip on the halfword): negative.** `Uint16 h; h = x; h = (Uint16)(Uint32)h` (h3a) = h2c (the narrowing of a 32-bit load is a real `clrlwi`); `w = -(-w)` (h3b), `w = (Uint32)(Sint32)w` (h3c: backend-00 shows `mr r36,r35`, propagated away at 02 — one web at the RA), `Sint32 w; SWAP32((Uint32)w)` (h3d): 6w, fold.
- **H4 (the value as a two-use own local): negative.** `Uint32 w = x; *val = SWAP32(w)` declared above/below elem, `register` (h4a-c): 6w, the word's colour is r4 in every declaration order (r4 is free, the vid does not matter). Own-local forms of the acc (statement-split `|=`) were pass 23's 7-8w.
- **The vendor's `+`/`^` trees do not give the r6 either:** SWAP32 with `+` (plus) / `^` (xor) instead of `|`: no pre-RA merge, the four terms are hoisted by the scheduler and the word takes r4 / r7 (8w each) — the "two accumulators live" picture of pass 37 needs the `|` tree.
- **Fold-side facts read this pass:** the fold does not gate on the WORD's liveness (`return (Bool)(w != 0)` g3: `stwbrx r6` + `neg/or/srwi`, word r6 because the neg temp takes r4 — a second live temp in B37 is what gives r6), nor on the displacement (`val[1]` f0: `addi r0,r5,4; stwbrx r4,r0,r0`), nor on the block (the same chain in the ENTRY block B0 folds, variant `entry`); `p = val + 1; p[-1] = ..` is folded by the frontend (f1/f2). Under `#pragma peephole off` + SFH_SWAP32_STORE (p0, size exact) the residue is 10w = `clrlwi r9`/`mr r7` order (2), the search loop's `lbz 0x18(r6)` vs `0x198(r8)` (2: the displacement fold is the POST-RA peephole pass 15 too — backend-14 still has `lbz r0,r6,0x18`, backend-15 has `lbz r0,r8,0x198`) and word r0 / acc r4 (6). Spelling the compare as `hdr[0x198 + i*0x40]` (p1/p2) gives the displacement but hoists the lbz above the `addi r6` (the target's `addi; lbz` order proves the vendor's lbz READ p at scheduling and was rewritten by pass 15 — the vendor's post-RA peephole ran on this very function and only the stwbrx rule did not fire) and breaks the other 18 functions. Pragma path closed (as pass 18b/23).
- **The stwbrx rule, read off GC/2.6 mwcceppc.exe (post-RA peephole = `call 0x500c30` at 0x434001; per-block driver 0x502b90 walks each block BACKWARD keeping 5 per-class live masks seeded from the block's live-out sets (0x5e1840 tables built by 0x5077b0: return registers live-out of the exit block, iterative liveness 0x507a60); rules per opcode from the list heads 0x5e10ec+op*4, registered in 0x500eb0..0x501923 with NO conditional — every rule always active; a rule returning nonzero restarts the opcode's list).** Fold rule 0x503180, registered for STW (0x5e11b0) and STWX (0x5e11b8): from the STW it follows the def table 0x5e10e8 ([pcode+0x10] indexed: `[id*4]` = the defining pcode of arg0, `+4` used for RLWINM) three times (each must be RLWIMI) and a fourth (must be RLWINM), requires every found pcode's arg1 register (`[p+0x34]`, the word) equal to the first's, the four (SH,MB,ME) = (8,24,31)/(8,8,15)/(24,0,7)/(24,16,23) each once (bitmask in edx), then scans the pcodes between the STW and the RLWINM for a WRITE of the word register (fails if found), then: STWX -> retag as STWBRX (0x35) copying the RLWIMI's arg1 in; STW with displacement 0 (`[esp+8]` set at entry when arg2 is an immediate 0) -> the 0x5035e1 path (`stwbrx w, r0, base`), else the addi path (f0). The dead chain is then removed by the driver's dead-def deletion (defs not in the live mask -> 0x4dcfb0) — which is why `return s | 1` keeps chain AND stwbrx (pass 23). Consequence: the rule is order-blind (def-chain, not adjacency), register-blind, block-blind and liveness-blind; its only C-reachable breakers are two source registers (a mask/extension node = code) or a consumer between the last rlwimi and the stw (code). The target's stream (one word register r6, the four masks, nothing between) satisfies every gate, so the vendor's pass 15 (which DID run: the `lbz 0x198(r8)` displacement fold is in the same function) either lacked this rule or saw a different def table — compiler-side. M4 stands; sfh_main stays 35/36, 6w. Not read to the end: the def-table builder (0x4dcdc0 / where [pcode+0x10] is assigned) and the meaning of `[block+0x2a] & 3` / `[pcode+0x14] & 0x18d` in the driver (the entry-block test showed they do not exclude B0's store).

### CRI pass 74b (adx_dcd5 Ste4AsSte 3w / Ste4AsMono 5w: the dead-write and coalesced-leader levers measured on the product/shift temps; IN PROGRESS; 2026-09-12)
Harness ~/.cache/cri74b/ (`mk.py NAME [--ste|--mono] 'OLD=>NEW'.. ['^ANCHOR=>LINE' inserts after, '@OLD=>NEW' edits the signature]`, `t.sh NAME [--sbs] [--ra F] [--sd F]` = both functions' words, `regs.py X [--mono]` = the edit that makes X `register`, `wi.py RA_DIR [--k N] [--all] ref=VID:rN edge= unedge= fixed=V:N ghost=P ghostlike=V:P vid=V:KEY del=V` = chaitin what-if scored against the dump's own colours with `ref=` corrections; deleted at the end).
- **Lever (1), a dead write into the already-pinned r6, measured on every value (all 24 locals/parameters, `register` added where needed — `register` alone is byte-neutral on all of them): for a value LIVE at the asm the write changes NOTHING (Ste 3w in 18/18 cases: the `mr r6, x` reaches the RA (it is in backend-17) and is deleted there with no interference effect, r6 already interferes with everything through the histl ghost); for a value DEAD at the asm it is a real live-range extension of that value back from its last use (rr2 116w, s/key 92w, j 167w, histr 106w, d 7w = d kept live across its own in-place `mullw`).** So the "ghost adjacent to what is live at that point" reading of pass 73 is wrong: pass 73's dead writes work only because their source is a PARAMETER whose entry copy `mr r38,r9` the propagation then keeps (the ghost is the parameter copy, entry -> the asm). No dead write can add a neighbour to a temp inside a statement.
- **Model tolerance (Ste, K=28 graph + `ref=121:r21 ref=122:r26`): `fixed=V:1` on any ONE node keeps 92/92 except V in {58 (d table web), 60, 62, 85, 92, 95, 98, 100, 102, 115 (d*sc_l), 117 (c2*l1)}; +1 on every node live in B26 (the rr1 statement) = 92/92; +1 on B20+B26 = 57/92.** So the original's extra neighbour of `c1*t` covers at most the rr1 statement's block; every value dead there (rr2, d, s, key) is also dead across B20, so no real value can supply it (confirms pass 73).
- **Web numbering read off 7 variants (products written into locals, kept as webs by an asm use): the `@N` of two webs defined in the same block follows the order of their LAST USES — the operand used later in the final expression gets the LOWER @ = the HIGHER vid** (`(d + s)` -> s = @65; `(s + d)` -> d = @65; independent of the statement order and of the asm order). Consequence for Ste's residue: with `s = c1 * t; d = c2 * rr1; ... ((s + d) >> 12)` the add's operand order (c1t, c2rr1 = the target's `add r21,r21,r26`) gives c2rr1 the higher vid — the opposite of what the target needs; `(d + s)` (w1) colours the two products right (r21/r26, mullw order right through the asm-consumer tie-break below) but emits `add rD, c2rr1, c1t` and evaluates the shift subtree before `dr*sc_r` (26w, plus s's header damage). A web of `rr1` itself (`rr1 = c2 * rr1`) is an in-place def of the own node (r29). Single-use arithmetic webs are substituted back into their use unless a second use exists (the asm dead write counts as a use, `asm { mr r6, d }` after the def).
- **Scheduler fact (sched.py rule 2 confirmed on the dumps): an asm dead write `mr r6, x` is a successor with npreds 1 of x's defining pcode, so x's def wins the pre-RA tie against an otherwise equal pcode ("frees more successors"); two such writes chain by a WAW edge on r6, so only the FIRST asm's source gets the +1.** This is a codeless way to order two tied independent instructions (the RA deletes the write), catalogue row "instruction order within a block".
- Mono (b) re-read: the shift temp r130 [srawi, add] is contained in the product r126's range [mullw, add]; a node adjacent to r130 and not to r126 cannot be a value; `unedge=126:129` (the product issued after the srawi) gives r126 r26 (target r27), `vid=126:130.5` nothing (r126 is L2 by degree 28, r130 L1 by 27): the original's r130 had 28 at scan 1.
- **Mono (a) read on the plain-web form (p1 = the tree's Mono with `key = sadd + key * smul`, no `ps`; 45w): sadd (own node r42) is removed in the own-local scan (L18) with 27 and pops LAST there (lowest vid) -> r24; the target's sadd (r0) pops before l2 (r12), so it is either L19 (`fixed=42:1` on the p1 graph = the target up to the B1 temps) or a node with a vid ABOVE the own locals = the family-(B) leader (the `ps` form's hoisted load r79 gets that vid, which is why the tree keeps `ps`).** The two Mono residues are one fact: the original's sadd node contains BOTH the entry load (issued before the `add`, so the `nfrm/2` add temp is adjacent to it and cannot take r0) and the loop value with a high vid = the frontend's hoisted widening copy `mr @140,r42` surviving to the RA and coalesced with the web as the leader. In ours that copy is folded (constant propagation turns the no-op `extsh` into `mr`, copy propagation forwards the single-def r42). The round trip (`sadd = (Sint16)(Sint32)sadd`) keeps the widening as a REAL `extsh` (two nodes: the load r42 pops in L1 and takes r22, the widening r0), so the add temp still takes r0 (i4 13w, i4n 10w, q1 51w).
- **B1 pre-RA order, controlled with the asm-consumer tie-break (all on p1): `asm { mr r6, nblk }` (register nblk) makes the `srawi` frees-1 -> issued right after the `add`, before every load (p2: add temp adjacent to nothing, `add r0` again, 45w); with the sadd/smul round trips (their `extsh` consumers = height 3) the srawi lands after smul/sadd and before scl/l1/l2 (i4n: `add r11`, 10w); adding `asm { mr r6, scl }` FIRST in the r6 chain (scl's load frees 1) gives the target's (D) order exactly — scl, smul, sadd, srawi, l1, l2, rr1, rr2 (q1) — but the colouring is the round trip's split (51w). The chain order matters: only the first `mr r6, x` of the function has npreds 1; the nblk write placed after the c1/c2 writes (p3/p4) leaves the srawi at frees 0 / height 2 = ahead of the plain loads by class -> 102w.** Dead writes of sadd into r6 (q7/q9/q10, 46w) move its load first but do not change its level.
- Ste (the +0x1d4 pair), what remains true after this pass: the two products are backend temps of one statement (creation order = the frontend's final tree order = the final instruction order), so with plain temps `c1*t` always has the lower vid and pops second; the target's `c1*t` therefore had one more neighbour alive at scan 1 (a node of the rr1 statement's block only). Not a value, not a dead-write ghost, not a `?:` copy on an own local (m2-m5: the `?:` copies into l1 stay as real `mr`s, 17-132w), not a two-web form (the add's operand order fixes which web is numbered higher). Frontend fact found on the way: the frontend reorders the operands of a commutative `+` (w1: `dr*sc_r + ((d + s) >> 12)` became `ESHR + EMUL`, evaluated shift-first, temps renumbered) — the rule was not derived (two EMULs are never swapped).
- Final state: nothing applied (adx_dcd5 2/4, Ste 3w + Mono 5w as found), objects.py untouched, nothing built under the lock, kit untouched. Harness ~/.cache/cri74b deleted. Next: a C shape whose `(long)sadd` hoist copy survives copy propagation as a `mr` (destination web two-def without code: a second hoist of the same widening, e.g. the widening used in two loops / two frontend hoisting levels) — that one node closes Mono's 5 words; for Ste, a pcode of the rr1 statement's block that the RA deletes AFTER `c1*t` is defined and before `c2*rr1` (a coalesced copy into a web/temp — no own local), or the vendor's clamp spelled so its join carries one.

### CRI pass 76 (adx_tsvr `adxt_nlp_trap_entry` 2w, `lha r4` vs `r0` in B16: the tagged-lever grid (r0 ghost, r4 pin, r0 neighbour pin, marked asm addi) — every zero-code form is negative by mechanism, the cheapest identity-of-B16 form costs +4 bytes (`addi r4,r4,0`); nothing applied, nothing flipped; 2026-09-12)
Harness ~/.cache/cri76/ (mk.py/t.sh as pass 74, wi.py = the pass-73 chaitin what-if, ra_* dumps; deleted at the end). Base dump chaitin `--check` IDENTICAL; t = r56 (nbrs r1 r3 r33 r36 r37 r38 r39 r53 r54), the r0-coloured nodes are r67/r66 (tail, 3 nbrs), r52 (ck.len), r41-r43 (li/lis temps): none live in B16.
- **Facts established this pass (all on the tree source + one edit, variant.sh + ra.py):**
  1. A single dead asm write into r0 (`register Sint32 ofst1; asm { mr r0, ofst1 }` before `ofst1 += ofst`, a1) removes r0 from K exactly like the two-way pin of pass 46: 11w (`lbz/li/lwz/add r0` -> r3/r4). Pass 73's "K unchanged" holds only for a register ALREADY named in another asm write.
  2. An asm READ of a physical register does not remove it from K, and a dead asm write is dead at graph time: `register Sint32 z; asm { mr z, r0 } .. asm { mr r11, z }` / `asm { mr z, z }` (a2/a3) -> z has degree 0/0 (not coalesced, no ghost), both `mr`s deleted, 2w. The asm pcodes are scheduled freely (the lha moved above `mr z, r0`), so an asm statement is NOT a scheduling barrier in this block.
  3. **A dead def never marks its rA web** — the graph builder skips dead pcodes: `register t = ofst, u; u = t + 4; asm { mr r11, u }` (c2) keeps `addi u,t,4; mr r11,u` to pass 09 (both deleted by the RA, size unchanged) but u is 0/0 and t has NO r0 neighbour -> r0, 2w. `asm { mr u, u }` as the use (c1) is deleted at pass 02 (CSE) and the addi swept at 04. So pass 74's "unexplored construct" (a def dying at pass 05) is moot: whenever a def dies before the RA, its rA operand carries no mark.
  4. A LIVE asm `addi u, t, 4` DOES mark t (d1: t gets r0 in its list, `lha r4`, +8 bytes of addi/subi) — pass 74's "asm pcodes carry no rA mark" was about `addic` (no r0 restriction in the ISA), not about addi.
  5. `addi X, t, 0` reaching the RA marks t but is NOT coalesced: the RA emits it as an instruction and pass 16 (post-RA peephole) does not delete it, even as a self-form. How to get one there: the 13-pass pipeline becomes the 17-pass one when a constant-foldable pcode is present (`asm { addi u, t, 0 }` e1, or an addi chain folded by add-propagation at 04 into `addi 0`, f2/i1/i2); const-prop (06) turns every `addi 0` it sees into `mr`; but an `addi 0` CREATED by the second add-propagation (09) survives to the RA: `asm { li k, 4; add u, t, k; addi v, u, -4 } ofst1 += v;` (k1) -> 06 const-props `add u,t,k` into `addi u,t,4`, 09 folds the chain into `addi v,t,0`, RA colours t r4, v r0: `lha r4; addi r0,r4,0; add r27,r27,r0` (11w). Self-form `asm { li k, 4; add u, t, k; addi t, u, -4 } ofst1 += t;` (k5) -> `addi r4,r4,0` between `lha r4` and `add r27,r27,r4` (10w = the 4-byte shift only; B16 otherwise identical). **This is the cheapest form that reproduces the target's colours; it is not identity (+4 bytes).**
  6. An asm copy FROM r4 coalesces without removing r4 from K (h1/h2: `asm { mr t, r4 }` -> t ≡ r4, `add r27,r27,r4`), but the lha then defines a dead web and is deleted (10w). No copy form reads both t and r4.
  7. Two dead writes into the same register pin NEITHER value (j1-j3: `asm { mr r4, t }` + `asm { mr r4, hi }` -> t r0, hi r5): pass 73's second write keeps a COPY, it does not pin; so the b1 pin (`asm { mr r4, t }`, 8w: the call-1 `lis` temp and the call-2 vtbl temp leave r4) cannot be repaired by re-pinning those two temps. Asm writes into r8-r11 (unused here) are dead and free (g1-g3, 2w).
- **Conclusion so far:** t's colour is "lowest free of r0,r3.." and nothing in B16 can be r0-coloured; the r0 exclusion needs either (a) r0 out of K (any asm write into r0: shifts 4 other r0 temps, 11w), (b) a ghost aliased to r0 live across the lha (needs a live consumer = an instruction, or a write into r0 = (a)), or (c) an rA mark from a pcode LIVE at graph time (dead defs do not mark) that leaves no instruction: the RA deletes only coalesced `mr`s (no mark) and dead defs (no mark); the post-RA peephole deletes only dead defs and folded pairs whose partner would have to be a target instruction with r4 as base (none). So no tagged lever of this compiler reaches 6/6 here at equal size; the vendor's `lha r4` came from a construct outside this compiler's reachable set (or a different build).

### CRI pass 77 (sfh_main `SFH_AnlyElemSmpHz` 6w, M4 `stwbrx` fold: the post-RA peephole DRIVER read to the end — its `[block+0x2a] & 3` / `[pcode+0x14] & 0x18d` tests gate only the dead-def deletion, the fold has no flag gate; the def table is PER BLOCK, so a block boundary between the chain and the `stw` is the one codeless breaker; IN PROGRESS; 2026-09-12)
Harness ~/.cache/cri77 (t.py NAME 'OLD=>NEW'.. as pass 75; pe.py = the PCode opcode table; drv26/rule26/pf26/or26.txt = objdump of the GC/2.6 driver 0x502b90, fold rule 0x503180, peephole-forward 0x501930, post-RA OR rule 0x503730; ra_*/sd_* dumps; deleted at the end).
- **Driver 0x502b90 (per block, called from 0x500c30 for every block with >= 1 pcode, after the def-table builder 0x5075f0):** seeds 5 per-class live masks from the block's live-out, walks the pcodes BACKWARD. Per pcode: `[pcode->block+0x2a] & 3` (block flag 0x1 = ENTRY block, 0x2 = EXIT block: B0 is `0005`, the `blr` block `0006`; 0x4 = every block, 0x8 = scheduled) -> not deletable; `[pcode+0x14] & 0x18d` (pcode flags: 0x1 branch-ish (BLR `02000081`), 0x4 store (STW `0024`, STWBRX `0024`), 0x80 VOLATILE (`*(volatile Sint32 *)val =` gives STW `00a4`), 0x8/0x100 = other keep bits) -> not deletable; a block with no predecessors -> delete; else a pcode none of whose written registers is in the live mask -> deleted (0x4dcfb0). Only a NOT-deleted pcode gets the opcode's rule list (0x5e10ec+op*4; the list is LIFO: the fold 0x503180 was registered first, so it runs LAST for STW after 0x504880/0x505a00/0x505ca0/0x5061a0), a nonzero rule restarts the list; then the live mask is updated (written regs cleared, read regs set). **So `& 3` / `& 0x18d` never stop a fold; they only keep the dead chain alive after a fold (the `return s | 1` chain+stwbrx of pass 23 = the acc live, not a gate).**
- **Def-table builder 0x5075f0 (per block):** `pcode->[0x10]` = running operand index; for every REGISTER READ operand `deftable[idx] = lastdef[class][reg]` (a dummy zeroed pcode when the register was not written earlier IN THIS BLOCK), then every WRITE operand updates `lastdef`. Purely intra-block, no flag tests. The fold rule 0x503180 follows `deftable[stw.arg0] -> RLWIMI.arg0 -> RLWIMI.arg0 -> RLWIMI.arg0 -> RLWINM` (the `+4`/arg1 branch is dead code: it is taken only for a pcode whose op is RLWINM, which the loop never visits), so a chain whose `stw` is in ANOTHER block than its last `rlwimi` never folds (the dummy's op is 0). Verified: `Uint32 w = SWAP32(x); z = 0; if (z != 0) w = 0; *val = w;` (variant e1) = chain kept, size EXACT 0x15c, 7w: the compare is folded by the same pass 15 (`li r0,0; cmpi; bt` -> `b`, CMPI/BT rules 0x506e00/0x5055a0..; c1's `== 0` polarity is NOT folded because the pre-RA scheduler hoists `li; cmpi` above the r0 chain), the emptied arm leaves a label = block boundary with fall-through; residue = word r3 (the `li r3,1` moved into the store block frees r3) and `li r3,1` after the `stw`. Every shape without a real conditional (`goto L; L:`, `if (0) goto`, `switch (0)`, `do {} while (0)`, `while (0)`, helper for the swap / the store / store+return) is cleaned by the frontend and folds (6w).
- **No post-RA `or`->`rlwimi` fusion exists** (the OR rule 0x503730 fuses only SLW/SRW pairs, ops 0x6a/0x6b -> rotlw); the MR rules are `li rA; mr rB,rA` (0x505320), `lwz rA; mr rB,rA` -> `lwz rB` (0x505060, fails on any read of rA in between), `mr rA,rA` delete (0x5073d0), swap-back copy delete (0x507290), gekko `mr`->`addi` (0x503e90). So a surviving copy in the chain cannot explain the target either. `#pragma peephole off` sets the global byte 0x5eb16f (gates BOTH the pre-RA forward 0x500e80 and the post-RA 0x500c30), not a pcode flag; asm pcodes carry the same flags as C pcodes.
- ADX originals: `stw r10,0(r5); lwz r10,0(r5); stwbrx r10,r0,r5` (adx_bau/adx_baif) = the vendor's compiler DID fold `*p = SWAP32(*p)`; so the Sofdec readers' unfused chains are a source-shape effect (a block boundary between value and store in a shared helper/macro), not a build difference.
- **Second half (the coalescing routes):** 8. an asm `li t, 1` in the then-arm with `SJ_UngetChunk(sji, t, &ck2)` (m1; also plain `t = SJ_CK_DATA` m2, and m1 under `#pragma opt_lifetimes off` m3) does NOT join the lha into the argument web: the frontend substitutes the single-use `t = ofst` into the add (t is 0/0, the lha is a backend temp again) and the arm's `li` is const-propagated into `li r4,1` — 2w, rest identical. A two-use `t` would need a second use = an instruction; an `asm { mr t, t }` use is a dead def (deleted with its use). 9. No compiler pcode of any dumped CRI function names physical r0 before the RA except the `bl`/`bctrl` clobber lists (grep of tools/research/mwccdbg/out/*/backend-00), so an r0 ghost cannot come from a compiler copy either. 10. k5 whole unit: `.text` 0xcd0 vs 0xccc (+4), 5/6 identical, chaitin `--check` IDENTICAL on its dump — not flippable.
- **State: adx_tsvr 5/6, 2w, tree untouched (no source edit, objects.py untouched, nothing built under the lock).** No tagged lever of equal size exists in the reachable set: every pin shifts other colours (r0 write 11w, r4 write 8w and not repairable, two writes into one register pin nothing), every zero-code use is dead (unmarked), every marked live use is an instruction (+4 at best: k5). Catalogue note for the MWCC row "a hard pin fixes one register and shifts unrelated ones": the mark/ghost alternatives are closed for a block-local temp whose only neighbour colours are callee-saved/r3 — the vendor's r0 exclusion on such a temp is compiler-side (as sfh_main's stwbrx, pass 75), and the unit stays open. ~/.cache/cri76 deleted; kit untouched.
- **sfh_main Matching 35 -> 36/36 FLIPPED, 111 OK: `SFH_AnlyElemSmpHz` 6 -> 0w.** Form (tagged `COMPILER-DIFF: M4 (dead conditional)`): `w = SWAP32(SFH_ELEM_SMPHZ(elem)); ret = TRUE; z = 0; if (z != 0) { ret = id; } *val = w; return ret;`. Three facts, one cause each: (1) the chain stays unfused because the `stw` is in the block after the folded conditional (per-block def table); (2) `li r3,1` sits next to the `lwz` because `ret = TRUE` is assigned BEFORE the compare (a `ret` with a second reaching def in the arm is not folded into `return ret`; the `mr r3,ret` copy coalesces, so the `li` lands in the swap block; a single-def `ret`/`return TRUE` puts `li r3,1` after the `stw` = e1/r1, 7w); (3) the word takes r6 because the dead arm READS `id`: `id` (physical r4) is live through the swap block at the RA and the arm is deleted only in pass 15. Without the `id` read the word is r4 (m1, 5w); `ret = (id != 0)` adds a setcc (m5). Frontend rules found: own-local constants are NOT propagated into relational compares (`z = 0; if (z != 0)` reaches the backend) but ARE folded in truth tests (`if (ret)`, `if (!ret)`), in `return ret` (single reaching constant) and in every inlined helper (a helper local `z = 0` or a constant argument is folded: h4-h7 6w); the pass-15 BT/BF rule 0x505910 folds only an ALWAYS-TAKEN `cmpi/cmpli cr0, li-def, K; bt/bf EQ` (condition bit 2) into `b` (c4/e1), never deletes a never-taken branch (c1 `== 0` polarity stays 14w); a dead block's `b`/stores survive (pcode flags 0x1/0x4 in the 0x18d gate: c4 leaves `b`), so the arm must contain only deletable ALU pcodes (`ret = id`).
- Answer to the pass-63 volatile question: `*(volatile Sint32 *)val = SWAP32(x)` sets pcode flag 0x80 on the STW only (`0024` -> `00a4`), which is in the dead-def gate and nowhere in the fold rule -> still folds (6w, size 0x14c). Pass 63's 20w was the `volatile Uint32 *` on the LOAD (four loads). No pcode flag stops the fold; `#pragma peephole off` is the global byte 0x5eb16f.
- Compiler builds: GC/2.6 = 2.4.7.107 (PE 2003-07-14), GC/2.7 = 2.4.7.108 (2004-07-22); the originals' "GC20Apr2004Patch1" is a build between them that we do not have. The ADX originals fold `*sfreq = SWAP32(*(Uint32 *)(p + 0xC))` to `lwz r6; stwbrx r6,r0,r27` (adx_bwav, Matching), so the vendor's build had the rule; the Sofdec readers' chains are source shape. Catalogue row (MWCC): "target keeps `rlwinm/rlwimi x3/stw`, ours folds to `stwbrx`" -> the stw in another block than the chain: a conditional the frontend keeps and pass 15 folds (`z = 0; if (z != 0) {..}`), return value assigned before it, the dying argument read in the dead arm. Follow-up (not done): the six `sfh_GetHdr*` readers (asm chain + `peephole off` + hdr r5 pin since pass 18b) should take the same C shape with `hdr` read in the arm (their blocked register is r5 = hdr).

### CRI pass 78 (adx_dcd5 Ste4AsSte 3 -> 0w APPLIED with a tagged neighbour copy, Ste4AsMono 5w unchanged; adx_dcd5 3/4, not flipped; continued in "CRI pass 78, part 2" below; 2026-09-12)
Harness ~/.cache/cri78/ (`mk.py NAME [--ste|--mono] 'OLD=>NEW'..` = base.c with exact-string edits inside one function body, `@OLD=>NEW` = file edit; `t.sh NAME [--sbs] [--ra FUNC]` = both functions' words + ra.py dump; `wi.py` = the pass-76 chaitin what-if; deleted at the end).
- Base dump re-read: chaitin `--k 28` exact in order/colours (cost lines differ only); Ste residue = r121 (`c1*t`, 27 nbrs, removed in scan 1 before r122 `c2*rr1` 26 nbrs) — `fixed=121:1` 92/92 confirmed. r121's neighbours minus r122's = only r54 (rr1); a +1 must be a node dying at the `c2*rr1` mullw (a coalesced copy of rr1 or of the widened c2) — nothing else is live in B26 between the two products.
- Negative this pass (Ste): `asm { mr r21, pt }` on a `register pt = c1 * t` = 102w (r21 leaves the colour set, pass 76 rule 1, the target uses r21 for five other temps); `pt = c1*t; asm { mr r6, pt } pr = c2*rr1; asm { mr r6, pr } rr1 = dr*sc_r + ((pt + pr) >> 12)` (any statement order, with or without pr's asm) = 5w: the frontend turns both statement products into `@N` temps + propagated copies (pt/pr 0/0), the @ numbering follows the LAST-USE order (pass 74b: the second add operand gets the higher vid = r21), and `dr*sc_r` (a backend temp, vid above every @N) is then coloured before both and takes r21; the mullw order IS right (the first r6 write's source wins the tie). `pr = c2 * rr1; asm { mr r6, pr } rr1 = dr*sc_r + ((c1*t + pr) >> 12)` = 3w with the RIGHT colours (c1t r21, pr r26) but pr's mullw issued first (input order) and the frontend SWAPS `c1*t + pr` into `pr + c1*t` (an @temp operand is moved before an expression operand: `add r21,r26,r21`). Inline helpers `c2 * adxdcd_id(rr1)` / `adxdcd_mul(c2, rr1)` (`static inline`, one-line bodies) = 3w: the parameter copies are substituted by the frontend (backend-00 reads r54 directly) — pass 63's surviving helper webs were multi-def INSIDE the helper.

### CRI pass 79 (adx_tsvr Matching 5 -> 6/6 FLIPPED, 111 OK: `adxt_nlp_trap_entry` 2 -> 0w (`lha r4` vs `r0` in B16) with the pass-77 dead conditional carrying an `addi` use of the lha temp to the RA; 2026-09-12)
Harness ~/.cache/cri79/ (mk.py NAME 'OLD=>NEW'.. exact-string edits of the tree file, t.sh NAME [--sbs] [--ra] = variant.sh words + ra.py dump; deleted at the end). Kit untouched.
- **Form applied (tagged `// COMPILER-DIFF: M1 (rA use of the lha temp kept to the RA by a dead conditional)`), declarations `register Sint32 t; Sint32 z;` after `ofst2v`:** `t = ofst; z = 0; if (z != 0) { ofst1 = t + 4; } ofst1 += t; ofst2v = ofst2;` in place of `ofst1 += ofst; ofst2v = ofst2;`. Size exact (0x2ec), the other 5 functions identical, `.rodata` pad as before. objects.py `"lib/adx_tsvr.c": True` (a `# CRI pass 79` block), `ninja -k 0`, `dtk shasum -c` = 111 OK.
- **The three effects verified on the ra.py dump (chaitin `--check` IDENTICAL):** (1) RA time: B16 = `li r33,0; lha r34,r1,0(ofst); cmpi cr0,r33,0; bt cr0,2,B18`, B17 (the arm) = `addi r38,r34,4` (ofst1's own web, no range split), B18 (the join) = `cmpi cr0,r55,0; lha r35,0(ofst2); add r38,r38,r34; bt` — the addi is present, t = r34 has physical `r0` in its neighbour list (10/11: r0 r1 r3 r33 r35 r38 r39 r40 r41 r55 r56) and colours r4; z = r33 (9/9, `li`+`cmpi` only, no mark) colours r0. (2) backend-12 (post-RA peephole): B16 = `lha r4; b B18` (the `b` to the fall-through block is dropped at emission), B17 has no predecessors and is empty, B18 = `cmpwi r28; lha r26; add r27,r27,r4; bt` = the target's order (the add already last in the join's pre-RA schedule). (3) The target's B16 `lha r4; cmpwi r28; lha r26; add; beq` is therefore two blocks in ours with the same instruction stream — the block boundary is harmless ONLY when it sits right after the lha statement (see p2/v1 below).
- **Grid (each one edit of the applied form; 0w = identical):** compare form `z == 1` 0w, `z = 1; if (z == 0)` 0w, `if (z)` 2w (truth test folded by the frontend, pass 77), `z > 0` 13w/+16 bytes (the pass-15 BT rule folds only the EQ bit: `bgt` stays real code); position: conditional after `ofst1 += t` (v1/p2) 3w — the boundary then separates the add from `cmpwi n1`/`lha ofst2v` (ours `lha r4; add | cmpwi; lha r26`, target `lha; cmpwi; lha; add`), `ofst2v = ofst2` before `ofst1 += t` (p1) 0w; inside the `n1 && n2` Unget arm (v3) 2w (that arm returns, so `ofst1 = t + 4` there is a dead def the frontend deletes: no addi); `t` plain 0w, `Sint16 t` 0w (register or plain); z `register` 0w, z declared before t 0w; `z = 0` at the function top (d1) 16w/+16 (z live across calls takes a callee-saved register and the `li` leaves the compare's block: the BT rule's def table is per block, so nothing folds); no `t` (`ofst1 = ofst + 4` in the arm, `ofst1 += ofst` after, d2) 2w. Consumer in the arm: `ofst1 = t + 4` 0w, `n2 = t + 4` 0w (n2 read by the later compare), `ofst2v = t + 4` 2w and `z = t + 4` 2w (redefined right after the join / unused -> the frontend deletes the dead def, no addi reaches the RA), `n1 = t + 4` 8w (n1's `mr. r28,r3` web disturbed), `ofst1 = t` / `ofst1 += t` / `ofst1 = t + ofst1` 2w (a copy / an `add`: no rA position, no mark — as ties 2). Rule: the arm's def must be of a variable that is READ later along the fall-through (a reaching def), must be an `addi`-family pcode (`+ K`), and the conditional must sit before the statement whose block order the target shows.
- **Natural conditional: none available.** The fold needs an always-taken `li; cmpi/cmpli; bt/bf EQ` in the compare's own block, i.e. an own-local constant compared relationally; every existing test of the function (`lnksw`, the DecodeFooter result, `n1`, `n2`, `GetStat`) is on a call result or a load, and `ofst2 = 0` is address-taken (a stack slot). The dead conditional stays as the tagged form.
- Lever catalogue consequence (MWCC rows "two values with swapped registers" / "a hard pin fixes one register and shifts unrelated ones", ties-2 note): a temp coloured r0 in ours and r4 in the target = the target web had an rA-position use; the codeless carrier is the pass-77 dead conditional with `X = t + K` in the arm (X read later), placed so that its block boundary does not split the target's block schedule. Passes 74/76's "no zero-code lever exists" was about levers deleted BEFORE the RA; this one is deleted at pass 15.

### CRI pass 78, part 2 (continuation of "CRI pass 78" above — the CRI pass 79 section landed between; adx_dcd5 Ste4AsSte 3 -> 0w APPLIED with a tagged neighbour copy `asm { mr x, t }`; Ste4AsMono 5w read, unchanged; adx_dcd5 3/4, not flipped; 2026-09-12)
- **Ste4AsSte 3 -> 0w APPLIED (locked ninja + bytecmp: adx_dcd5 3/4, .text size equal):** `register Sint32 t; register Sint32 x;` and, right before the rr1 statement, `asm { mr x, t }` with `c1 * x` in the prediction and `rr2 = x;` at the body end (tag `COMPILER-DIFF: M1 (neighbour copy)`). Mechanism read off the dump (ra_a_x): `t` has three reaching definitions at the copy (the clamp arms), so backend copy propagation keeps `mr x,t` (pass 72's rule: a copy survives when its SOURCE has several reaching defs); `x` is an own local (not coalesced, 26/30, L>1 so it is not swept in scan 1) live from the copy to `rr2 = x` = one more neighbour on `c1*t` (28, held one scan longer -> popped before `c2*rr1` -> r21) and on `c2*rr1` (27, still L1); `t` dies at the copy so `x` takes the lowest free register = `t`'s r20 and the copy is `mr r20,r20`, deleted after allocation; `rr2 = x` is the original's `mr r30,r20`. Variants: `c1 * t` kept + `rr2 = x` (a_x3) also 0w; `c1 * x` without `rr2 = x` (a_x2) 3w (`x` dies at the mullw: no neighbour); a plain C copy `s = t`/`d = t`/`key = t` + `c1 * s` + `rr2 = s` 3w (the frontend substitutes every variable copy, whatever the use count); `rr2 = t` moved before the statement + `c1 * rr2` 18w (a real `mr r30,r20` in B26: an own-local copy never coalesces and rr2 is loop-carried, no range split). **Catalogue row: "a temp must be coloured one level earlier and a REGISTER is free in that block in the target (the dying value's register)" -> `asm { mr x, v }` from a MULTI-reaching-def `register` variable `v` whose last use is the copy, `x` used where `v` was and further down: a self-move deleted post-RA, a real node coloured `v`'s register = one more neighbour on everything live between the copy and `x`'s last use.**
- **Ste4AsMono 5w unchanged (read with the new lever, nothing applied):** (b) the l1 statement's shift temp r130 (27 nbrs, L1) must pop before the `d*sc_l` product r126 (28, L2; `fixed=130:1` 109/110, `fixed=130:1 fixed=126:1` 107/110 — the +1 must be on the shift ONLY). The shift's range [srawi, add] lies inside the product's [mullw, add], so the extra node must be born after the mullw; in the target's l1 block only r26 (the sum, dying at the srawi) and r29 (old l1, dead since `c2*l1`) are free there, so the original's extra node was a copy of the sum living past the srawi or a copy of the product consumed by the add — neither has a multi-reaching-def source (the neighbour-copy lever needs one: `d = d * sc_l; asm { mr s, d }` / `key` (m6/m7) is folded — one reaching def at the copy — and renumbers the header webs, 33/41w). Statement forms `pq = d*sc_l; sh = (..) >> 12; l1 = pq + sh` + dead writes `asm { mr r6, sh }` / `asm { add r6, sh, l1 }` after the add (m3-m5) = 5w: a DEAD asm does not extend liveness (the graph builder skips dead pcodes — the shift stays 27/27 even with the asm scheduled after the add), which also corrects pass 74b's "live-range extension" reading of dead writes of dead values (that effect was the scheduler's, not the graph's). (a) the `nfrm/2` add temp r0 vs r12: B1's final order is identical; the target's add temp had an r0-coloured neighbour = sadd's load issued BEFORE the add in the pre-RA order (post-RA the WAR edge on r0 pushes `lha r0` after the add, pass 71); ours hoists the load into the preheader (`ps`), the plain-web form puts it in B1 after the add (pass 74b). No multi-reaching-def source exists in B1 for the neighbour-copy lever. Both Mono residues stay as pass 74b read them.
- Final state: src/lib/adx_dcd5.c ADX_DecodeSte4AsSte 3 -> 0w APPLIED (register t/x + `asm { mr x, t }`, comment updated), Mono untouched; adx_dcd5 3/4 (Mono 5w), not flipped, objects.py untouched, no `ninja -k 0`. Harness ~/.cache/cri78 deleted; kit untouched.

### CRI pass 80 (adx_dcd5 Ste4AsMono 5 -> 2w APPLIED: the l1 statement's shift coloured before the `d*sc_l` product by a dead conditional INSIDE the l1 clamp whose arm's `addi` marks the shift no-r0; the `nfrm/2` add temp (r0 vs r12) read to a pre-RA B1 order that every colour reproduces (w3) but whose smul/sadd load order cannot be met at the same time; adx_dcd5 3/4, not flipped; 2026-09-12)
Harness ~/.cache/cri80/ (`mk.py NAME 'OLD=>NEW'..` exact-string edits of the Mono body of base.c; `mka.py NAME --plain|--looptop S|--pre S|--post S|--regsadd|--regscl|--sub OLD NEW` edits of the applied q5 form; `t.sh NAME [--sbs] [--ra] [--sd]` = variant.sh words + ra.py dump ra_NAME + scheddump sd_NAME; `wi.py RA_DIR [--k N] ref=V:rN fixed=V:N edge=A:B unedge=A:B vid=V:KEY node=V:adj..` = chaitin what-if scored against the dump's colours with ref= corrections; deleted at the end). Base dump chaitin `--k 28 --check`: order/colours IDENTICAL (cost lines only).
- **(b) APPLIED (src/lib/adx_dcd5.c ADX_DecodeSte4AsMono, locked ninja + bytecmp: Mono 2w, Ste 0w, adx_dcd5 3/4, size equal, .rodata OK; objects.py untouched):** declarations `Sint32 sh; Sint32 sum; Sint32 pq; Sint32 z;` after `ps`, and in place of `l1 = d * sc_l + ((c1 * l2 + c2 * l1) >> 12); ADX_CLAMP(l1);`: `pq = d * sc_l; sum = c1 * l2 + c2 * l1; sh = sum >> 12; l1 = pq + sh; if (l1 > 0x7FFF || l1 < -0x8000) { z = 0; if (z != 0) { rr2 = sh + 1; l2 = sum; } <the clamp's inner if/else> }` (tag `COMPILER-DIFF: M1 (dead conditional)`). Mechanism read off the dumps: the model wanted `fixed=130:1` (one never-removed neighbour on the shift ONLY) = 109/110; the arm's `addi r55,r43,1` gives `sh` the physical r0 in its list (pass 79's mark) = exactly that; `sh` and `sum` become own locals (2 uses each) live into the clamp arm (+l1, +z), the product stays a backend temp. Pop order shift (r20), sum (r26), product (r27) needs removal order product < sum < shift: the product (temp, vid 131) is removed in the same scan as the L14 temps, the sum/shift (own locals, degree 30-31 with the extension) one scan later, sum before shift because `sum` is declared AFTER `sh` (lower vid). Without `l2 = sum` in the arm (q7) the sum is a temp created BEFORE the product (`sh` statement first) and pops after it (6w); with the plain expression `l1 = d * sc_l + sh` the frontend puts the VARIABLE operand first (`add r29,r20,r27`, 3w) — the cheaper operand is evaluated first (a variable/@temp before a multiply; the base's `product + shift-of-products` kept the written order because the shift subtree was the costlier); `pq = d * sc_l; l1 = pq + sh` keeps (product, shift) even though the frontend substitutes the single-use `pq` back (no `[pq]` in frontend-01: the substitution keeps the operand position). `(int)pq` / `(Uint32)pq` / `int pq` (q1-q3) 2w too.
- **Placement lesson (z's register):** the same conditional placed right after the shift in the l1 statement's block B26 (b1) = 77w/+16 bytes: the pre-RA scheduler hoists `li z,0` to the block top, so z interferes with everything live in B26 (31 nbrs >= K) -> spill pick -> a NEW callee-saved r17 + frame. Inside the clamp's outer-test arm (the block after `bgt`) z has <= 29 nbrs and finds a free register (r27, the dead product) -> deleted. Rule: put the dead conditional in a block where a register is free for `z` across the whole block (its `li` is scheduled first, its `cmpi` last), i.e. never in a block that is full at its top; a value needed in the arm may be extended into that block if the target has its register free there (sh r20 / sum r26 are free in the clamp arms).
- **Negative on the way:** `asm { mr r6, pq }` inside the loop to keep `pq` a variable (c3) = 138w (a dead r6 write inside the loop = pass 73's whole-loop ghost); `sh`/`sum`/`pq` all three extended into the arm (p1/p3) = 59w (z has no free register: r20/r26/r27 all taken -> r17); the product as a frontend CSE @temp (`l2 = sum + d * sc_l` in the arm, p2) 142w.
- **(a) the `nfrm / 2` add temp r0 vs r12, read to a structural dead end (nothing applied):** the target's own colours forbid the "sadd's load before the add" reading of passes 71/74b/78: `srwi r0` (the add's operand r73, live until the add) and `lha r0` (sadd) are both r0, so sadd's load was NOT issued between the srwi and the add in the original's pre-RA order either (they would interfere) — the add temp's r0 exclusion is an rA MARK (a deleted addi/load/store reading it), and r11's exclusion is scl's load before the add (the only r11 node). Facts measured: (1) a dead conditional whose `li/cmpi/bt` land in the ENTRY block leaves `li r0,0; cmpwi r0,0` as real code (+8, x3): the post-RA driver never deletes dead defs in a block flagged entry (pass 77's `& 3` gate), and a conditional placed first in the function is merged into B0; any later B1 position splits B1 so that the hoisted `lis/addi`, the c1/c2 `extsh` and `li i` land in the join block (post-RA order broken) or the entry loads in the compare block; so no dead conditional can mark a B1-local temp. (2) `asm { mullw r6, sadd, smul } asm { mr r6, scl }` (x2, plain-web form) puts all three stack loads before the add (a two-source dead asm gives both loads height 5 = urgent at c0; the chained `mr r6, scl` is urgent at c2 by height 3 even with npreds 2 — pass 74b's "only the first r6 write" is about frees, height still counts): scl r11 but smul/sadd swap (both get the r1/r72 pair (+2), equal cost 33 and degree -> the spill pick takes the higher vid sadd), and the srwi temp leaves r0 (the r0 node is live at it) -> 16w. A `register Sint32 sw = sadd; asm { mr r6, sw }` (a4) is propagated (`mr r6,r42`): pass 73's kept copies work only for a PHYSICAL source (`mr r6,r9` is refused, `mr r6,r42` is not). A second def of sadd in a dead arm (loop top a2 / after the loop a1) keeps `extsh @W,r42` as a REAL extsh (constant propagation folds the widening into `mr` only for a single-def source; after the loop the arm's def does not reach the preheader copy at all). So the vendor's add temp mark has no codeless C carrier in this block; Mono stays 2w.
- Tree (interim): src/lib/adx_dcd5.c ADX_DecodeSte4AsMono (declarations, the l1 statement + clamp, comments); adx_dcd5 3/4 (Mono 2w), not flipped, objects.py untouched, no `ninja -k 0`.
- **(a) second half — the target's B1 read correctly and a 2-word alternative found, not applied:** Ste's own dump corrects the reading above: the add temp is live from the `add` to the `srawi`, and the `srawi` is scheduled LATE (c9, after every load: it has height 1 and loses to the loads (h2) and the class-0 `mr`s), so in Ste the add temp is adjacent to scl/smul/sadd AND the hist loads (-> r19 = the first free callee-saved); in the target's Mono the add temp is r12, i.e. adjacent to sadd (r0) and scl (r11) but NOT to l2 (r12): the original's pre-RA B1 had `add < lha sadd < srawi < lha l2` with `lwz scl` before the srawi — no mark needed (my "r73/sadd both r0" contradiction assumed the srawi right after the add). Pieces that each work: (1) `register Sint32 nblk; asm { mr r6, nblk }` gives the srawi height 2 -> it is issued right after the add (c4/c5, class 2 beats the hist loads) -> the hist loads no longer touch the add temp; (2) a dead asm whose second source is the add temp itself gates a load's "frees 1" on the add: `nh = ((Uint32)nfrm >> 31) + nfrm; nblk = nh >> 1; asm { add r6, sadd, nh }` (register nh, sadd) -> `lha sadd` is picked right after the `add` in the same cycle (h3 > h2) and before the srawi; (3) a two-source dead asm `asm { add r6, scl, smul }` gives BOTH loads height 3 (urgent at c1) with no WAW chain between them -> scl c1, smul c2 in parameter order; (4) sadd's node reaches L21 (pops before scl -> r0) only with the r1/r72 argument-base pair (+2 = the pair AND the ghost; `fixed=42:1` alone is the model's minimum) = sadd's load must not be the LAST use of the argument base, AND its spill-pick tie against smul must be lost by sadd: a dead-arm read of `sadd` in the l1 clamp arm (`l2 = sum + sadd`, weight 64: cost 33 -> 161) does that. **w3 = (1)+(2)+(4) with `pm = &smul` (the hoisted `*pm` load is the 4th argument-base use, issued last) reproduces EVERY colour (`add r12; srawi r18,r12`, sadd r0, smul r19, scl r11) = 3w: the add's operand order (fixable: `((Uint32)nfrm >> 31) + nfrm`) and the post-RA order of `lha r19`/`lha r0` (smul's hoisted load is after sadd's in the pre-RA output, post-RA ties keep the input order) = 2w, no better than the tree's 2w.** The contradiction that closes it: the target's post-RA order (scl, smul, sadd) needs sadd's load LAST in the pre-RA output, and sadd's level needs a later argument-base use — with three stack parameters the last load gets no pair, and the 4th use (`pm`) is always issued last (frees 0, appended by loop code motion). The vendor's sadd had its level from the coalesced widening leader (a high vid, no pair needed), which no C shape of ours keeps (multi-def source -> real `extsh`; vreg source -> propagated even into `mr r6, x`).
- Facts for the catalogue (dead conditional, MWCC): (i) its `li`/`cmpi` are deleted only in a block that is neither the ENTRY nor the EXIT block (flags 0x1/0x2 gate the post-RA dead-def deletion): a conditional placed first in the function is merged into B0 and leaves `li r0,0; cmpwi r0,0` (+8); the post-loop position works only because the compare block is split from the `blr` block. (ii) `z` is a real node from its `li` (hoisted to the block top by the scheduler) to its `cmpi`: the block must have a register free across its whole length or z takes a new callee-saved (+stmw/frame); z is removed early (cost 0 spill pick or degree) so it is never the "+1 never-removed neighbour" for anything (v2). (iii) the arm's own-local read gives the read value's node the r0 mark only for addi-family pcodes; `xori`/`add`/`mullw` reads mark nothing and only add cost. (iv) asm writes into r2/r13 are NOT dead (emitted as real `mr r2,..`), so the r6 chain is the only codeless consumer set and it serialises: only one load per chain position gets "frees 1" on time — the two-source `add r6, a, b` form is the way to give two loads priority at once.
- Final state: src/lib/adx_dcd5.c ADX_DecodeSte4AsMono 5 -> 2w APPLIED (Ste 0w, adx_dcd5 3/4, .text size equal, .rodata OK), not flipped, objects.py untouched, no `ninja -k 0`; tags: `COMPILER-DIFF: M1 (dead conditional)` on the l1 statement; the function comment updated. Harness ~/.cache/cri80 deleted; kit untouched. Residue 2w = `add r0,r0,r4` / `srawi r18,r0,1` (target r12).

### CRI pass 81 (adx_dcd5 Matching 3 -> 4/4 FLIPPED, 111 OK: ADX_DecodeSte4AsMono 2 -> 0w — the `nfrm / 2` add temp r12 by a B1 slot read of the address-taken addend that the loop's hoisted load CSEs into, gated on c2's widening; the last two words of the CRI library; 2026-09-12)
Harness ~/.cache/cri81/ (`mk.py NAME [--src F] 'OLD=>NEW'..` exact-string edits of the Mono body of base.c (`^ANCHOR=>LINE` insert after, `*OLD=>NEW` replace all, `@OLD=>NEW` file-wide); `t.sh NAME [--sbs] [--ra] [--sd]` = variant.sh words + ra.py dump ra_NAME + scheddump sd_NAME; `wi.py RA_DIR [--k N] [--nb] edge= unedge= fixed=V:N ghost=V:P vid=V:KEY del= cost=` = chaitin what-if scored against the Mono role map; `trace.py RA_DIR [--k N] VIDS..` = per-scan degree of the given nodes at examination time, `NB=<scan>` prints their alive neighbours; deleted at the end). Kit untouched. Base dump chaitin `--k 28 --check` exact (cost lines only).
- **APPLIED (src/lib/adx_dcd5.c ADX_DecodeSte4AsMono; locked ninja + bytecmp IDENTICAL 4/4, .rodata OK; objects.py `"lib/adx_dcd5.c": True` in a `# CRI pass 81` block; `ninja -k 0`, `dtk shasum -c` = 111 OK):** `register Sint16 *scl, register Sint16 smul` in the definition, `register Sint32 nblk; register Sint32 x; register Sint32 y;`, and after `ps = &sadd;`: `x = c2 + *ps; y = (Sint32)scl + smul; asm { mr r6, y } asm { mr r6, nblk } asm { mr r6, x } asm { mr r6, c1 } asm { mr r6, c2 }` (the pass-73 c1/c2 writes moved into this chain), tag `COMPILER-DIFF: M1 (dead consumers, entry-block order)`. `nblk = nfrm / 2`, `ps`, the l1-clamp dead conditional and the post-loop `asm { mr r6, l1 }` pin unchanged. x, y and the five r6 writes are dead defs deleted by the RA (0/0 nodes, size exact).
- **Why it is identical (read off ra_v13 / sd_v13, chaitin exact, sched.py pre+post IDENTICAL):** (1) `x = c2 + *ps` puts a slot load of the address-taken addend into B1 as a BACKEND temp (`lha r82,r79,0(sadd)`, the deref inside an expression); backend-14 CSE then merges the loop's hoisted `*ps` load (appended to the preheader by loop code motion, pass 71) into this FIRST occurrence — so the loop value keeps the hoisted temp's level (vid 82 above the c1/c2 widenings r70/r72 -> popped right after scl -> r0, exactly as the tree's `ps` form) but its load now sits at a STATEMENT position in B1 with a consumer. (2) The consumer `add x, c2w, sadd` has two preds: the load and c2's widening `extsh r70,r39` (the frontend reuses the hoisted @144 for the B1 read; the extsh gets height 3 and is issued at c6 — the target's `extsh r10,r10` is early too). Until the extsh issues the load "frees 0", so at c3/c4 it loses to the frees-1 parameter copies `mr r38,r9`/`mr r39,r10`, the `y` add and the two pool `lis` (all urgent at c3, maxheight 5); at c5 the table `lis` (frees 1) takes slot 1 and the load (h4) slot 2. Schedule: c0 `mr nfrm; mr histl`, c1 `mr c2; lwz scl`, c2 `rlwinm; lha smul`, c3 `add; mr c1`, c4 `add y; lis 0x6666`, c5 `lis AdxQtbl; lha sadd`, c6 `extsh c2; srawi`, then `lha l1 .. rr2`, the c1 extsh, the addi pair, `li i`. (3) Consequences in the graph: the add temp r81 [add c3, srawi c6] is adjacent to sadd (r0), scl (r11), both lis, NOT to l2 -> r12 (target); the sadd load is the LAST use of the argument base, so smul (not sadd) carries the r1/r79 pair: smul 47 vs sadd 45 at the spill pick -> smul is picked (33/47 = 0.70 < 0.73), sadd stays a top-level node -> r0 (pass 80's "spill-pick tie" solved by the load order alone, no dead-arm read); both `lis` are born BEFORE the sadd load, so they keep the argument-base pair (+2: r1 and its ghost r79) -> degree 28/30 at scan 1 = spill picks with cost 0 -> tie -> highest vid (table) picked first -> popped last -> magic r20, table r21 (the base's mechanism; without the pair both are 27 = removed in vid order = swapped, 4w). (4) `y = (Sint32)scl + smul` + `mr r6, y` FIRST in the r6 chain gives the scl/smul loads height 4 (2 + add + mr) = the sadd load's height, so the three tie and issue in input (parameter) order: scl c1, smul c2, sadd c5 — post-RA `lwz r11; lha r19; lha r0` as the target. `mr r6, nblk` second in the chain gives the `srawi` height 2 -> issued right after the sadd load, before the hist loads (the add temp not adjacent to l2). The chain order matters: `nblk` first (v18) 4w (the srawi frees 1 -> c3, before the sadd load).
- **Census (each = the applied form with one change):** without `y` 60w (loads unordered, sadd first); gate on c1 instead of c2 (`x = c1 + *ps`) 58w (c1's copy `mr r38` is at c3 -> the load frees at c3); gate on nh (`x = *ps + nh` with `nh = sr + nfrm; nblk = nh >> 1`) 5w = every colour right but the two `lis` swapped (the load at c3, before the lis: no base pair; `fixed`-free: the model needs +1 on the magic lis); gate on nblk (`x = *ps + nblk`) 6w (the srawi inherits height 3 -> urgent at c3 -> before the load); `x = *ps + y` 6w (the y add gets height 3 -> scl/smul height 5 -> urgent at c0 -> `mr histl` lands in c2's IU1 and the `add` is blocked by the IU forwarding rule (sched.py canIssue) -> the load issues at c2 before the add: `srwi r12`); `mr r6, x` moved after the loop 50w/+4 (x becomes a real value, r17: a post-loop r6 write is not dead there); no `mr r6, nblk` 6w; `x = *ps + c2` (operand order) also 0w; `nh = ((Uint32)nfrm >> 31) + nfrm` gives `add r12,r4,r0` (the frontend puts the variable first), `sr = ..; nh = sr + nfrm` keeps (shift, nfrm) — moot with `nfrm / 2`.
- **Negatives that close the task's hypotheses:** (H1) `Sint32 sadd_w = sadd;` + a loop-top dead conditional `z = 0; if (z != 0) { sadd_w = l1 + 1; }` DOES keep the copy `mr r60,r42` to the RA (two reaching defs at the use; codeless, size equal) — but an own-local destination is NEVER coalesced (SWAR pass 16's rule holds for a multi-def dest too: r60 and r42 both plain nodes, the copy became `mr r12,r12`), and the own local's vid (60) is below the c1/c2 widenings (70/71): in the final scan (L20) the own locals and the widenings are removed together in vid order, so the widenings pop first and take r0 (`extsh r0,r10`, 60w); `vid=60:72` in the model = 20/22, `fixed=60:N` never (N<=8 no change, N>=12 -> a cost-0/degree spill pick). trace.py: at scan 19 the widenings have 29/31 alive neighbours (6 phys params + 5 ghosts + 9 own locals + scl + sadd_w + 4 mix temps + 2 pool addi), sadd_w 35 — no C-visible +10. So "the coalesced widening leader" is not reachable from an own local; the reachable high-vid carrier is a BACKEND temp (a deref inside an expression, or the hoisted load), and the CSE direction (first occurrence wins) lets a B1 statement own the loop's load. (H3 as stated) `asm { add r6, sadd_w, nh }` on an own-local copy adds nothing over (H1). A second def of the PARAMETER `sadd` in a dead conditional before the loop (q1-q3) = `li r12,0; cmpwi` real code (+12: entry-block position or a B1 split) plus a real `extsh r0,r22` (the widening of a two-def source is not folded), as pass 80 said.
- **Facts for the catalogue (MWCC):** (a) backend-14 CSE across the merged B1/preheader block keeps the FIRST occurrence of a slot load: a `x = <expr with *p>` statement before the loop makes the loop's hoisted load a B1 backend temp at the statement's position — "the hoisted load cannot reach the parameter position" (pass 71) is answered by giving it an earlier twin. (b) A dead two-pred consumer `x = a + *p` gates the load's "frees 1" on the OTHER operand's def: choose the operand whose def is issued exactly where the load must follow (here c2's widening); gating on a value of the critical chain (`nblk`) raises that chain's height and moves it instead. (c) Pool `lis` temps (cost 0) are spill picks only while their degree is >= K at scan 1: born before the last argument-base load they have the r1 + ghost pair (+2) and pop in the target's order (highest vid picked first = popped last); born after it they are removed in vid order and swap. (d) The scheduler's IU forwarding rule (a class-2 op cannot issue into the only free IU if it reads the GPR written by the op that completed in the other IU at the end of the previous cycle) is what blocks `add` right after `rlwinm` when a `mr` takes IU1 in that cycle — a height change elsewhere can push a load ahead of the add through it. (e) A dead `asm { mr r6, v }` const-propagates a known-constant `v` (`i = 0` -> `li r6,0`): it is not a consumer of `li i`. (f) An own-local copy destination with two reaching defs keeps its `mr` to the RA (codeless second def from a loop-top dead conditional) but does not coalesce; the copy is deleted only when both sides get the same colour.
- Final state: src/lib/adx_dcd5.c ADX_DecodeSte4AsMono 2 -> 0w APPLIED, adx_dcd5 4/4 IDENTICAL, `"lib/adx_dcd5.c": True` (# CRI pass 81), `ninja -k 0` clean, `dtk shasum -c` 111 OK; tags in the unit: M1 (neighbour pin) x2, M1 (kept parameter copies), M1 (neighbour copy), M1 (dead conditional), M1 (dead consumers, entry-block order). Harness ~/.cache/cri81 deleted; kit untouched. No CRI residue remains in adx_dcd5.
