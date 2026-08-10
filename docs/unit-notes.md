# Per-unit matching notes

Notes recorded when a unit reached byte identity: which source shape the compiler needed and why.
They were kept next to the unit list in `config/G4BE08/objects.py` / `modules.py` during the work and
were moved here when every unit matched. Units without an entry matched without a note. The mechanisms
cited are explained in `docs/matching.md` (lever catalogue) and, pass by pass, in `docs/research/`.
Notes that describe an asm-emitted instruction or a hard-register `asm { }` pin record how the unit was
first closed; on 2026-09-17 every such construct in the game code and all but one in the CRI libraries
were replaced by C (`docs/research/asm-removal.md` has the recipe per site). Those lines are marked
"superseded"; the source comments at each site describe the C shape now in the tree.

## `Tools/db_mod.cpp`

- same source as t_esp/db_mod, identical since that flip

## `Tools/t_esp_area.cpp`

- the cDbgEditWindow ctor's r28/r29 tie closed by one codeless RA-time anchor after strlen (dbg_tool.h); the &path1/&path2 PRE pair re-tied by a fourth codeless ref in ToolEspArea; t_event's SubToolMessInit keeps its PRE numbering through a dead-test table-size lever. t_esp_area 38/38, t_lightarea 38/38 (its 4 words were the linkonce vtable relocs).

## `Tools/t_lightarea.cpp`

- the cDbgEditWindow ctor's r28/r29 tie closed by one codeless RA-time anchor after strlen (dbg_tool.h); the &path1/&path2 PRE pair re-tied by a fourth codeless ref in ToolEspArea; t_event's SubToolMessInit keeps its PRE numbering through a dead-test table-size lever. t_esp_area 38/38, t_lightarea 38/38 (its 4 words were the linkonce vtable relocs).

## `game/Espgen42.cpp`

- 16/16: Espgen42_Move00 9 -> 0. Loop B's bump index with the i term first (`jx * ((mx + 1) >> 3)`, jx/mx function-level: the nx chain sits in the Z block after the i-division branch as in the target), a tagged `register int jq asm("r0")` (the r0 occupant that keeps t_i off r0 -> r9) and three tagged codeless `asm("" : "=m"(v.x/y/z) : "r"(jx))` sched1 slot fillers (priority 90 through the `lfsx nrm[k].x` alias dependence, ready at t2: they hold the t2/t3 issue slots, delay the byte's load to t4 and the fast-cast loadaddr past the `mullw`, and push `xoris j` behind the first index add). Remaining tags in Move00: loop-A dead test (loop.c insn_count), loop-B asm pair (move_movables), the jq pin, the three fillers

## `game/Espgen43.cpp`

- 11/11: AddSandPower 3 -> 0: `stfs Add_power` is an asm with a `"r"` input pinned to r11 (COMPILER-DIFF: asm-emitted stfs, sched2 slot): after reload the asm is anti-dependent on `lwz r11, 8(r7)` (prio 5 + 1 = 6 -> first cycle, second slot, ahead of `lis Chk_pos@ha` prio 5); before reload the load writes a pseudo, so sched1 (stfs at cycle 3 slot 2, prio 3) and the local-alloc order W0 > W4 > W8 > high > addi are unchanged. The plain C store has identical dependences in both passes (the `*pos` loads are exempt as fixed scalar vs varying struct), so no C spelling separates the two schedules — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `game/EtcModel.cpp`

- EtcModel: GetEtcAmbType's first `return 1` copy kept with an empty `asm volatile("")` opening the else arm (COMPILER-DIFF 6, see the source comment)

## `game/act_btn.cpp`

- 10/10: checkButton 18 -> 0: cases 9/0xA are separate `break` nodes with a codeless `asm volatile("")` in the case-9 arm (COMPILER-DIFF 6): the real insn keeps group_case_nodes from merging 9/0xA into a range and blocks jump2's `x = a; if (c) goto l` hoist on the case-7 tail once the shared `li r3,0; blr` block is adjacent

## `game/at_mod.cpp`

- ObaLineHitChk 9 -> 0: the `&p0` argument after getPartsPtr is an asm-emitted `addi %0,r1,8` (cse folds a C `&p0` into the copy address pseudo across the call through the beq AROUND path, COMPILER-DIFF: 12); the den f0 anchor takes `"f"(de * ef)` (ready one cycle later, so the hoisted `mr r3,r27` keeps its slot) plus a second `"f"(den)` reference (keeps den above the product in local-alloc, COMPILER-DIFF: 13)

## `game/at_sub.cpp`

- at_sub: the sphere/capsule end-point distances through one twice-set `d` (no tie to the dz chain), At_poly_sphere_ck2's `hit = 99` after the fabsf asm, `hit = 0` inside the surface-hit arm and the rim-hit arm's `goto hit1` into the surface-hit `hit = 1` (the original cross-jump survivor)

## `game/block.cpp`

- collision / model helper units (atari, cloth, ik, mirror, pendulum ...)

## `game/cam_ctrl.cpp`

- 84/84: HermiteExport 142 -> 0 zero code (`((u8*) &tmp)[n]` byte copies on one function-scope `n`, case 2/3 loads v,v0,v1, `cut->num - 1 == k`, pad loop on `j` with an in-place `rem`); cameraHitCheck 125 -> 0 (`do { } while (0)` LOOP_END anchor breaks cse1's ebb at the hit join, `register cAtariInfo* at asm("r29")` + launder so the copy loop runs on the ctor's `this` temp and `at` is not cprop'd, `hit = 0` after the p copy; COMPILER-DIFF); r0_RailBehind 3 -> 0 (asm `la` for the three VecLinearCombination pointer args, COMPILER-DIFF: #3)

## `game/cam_extra.cpp`

- 43/43 zero code, no tags. CameraBinocular ctor 17 -> 0 (`PlRef(pPL)->getPartsPtr` makes the pPL load wait for the member stores; dying-store order permuted), CameraPushObject::move 62 -> 0 (`getColumn(inv, 2, ..)` frame-offset-0 pointer shape, single VecAngle call, `!(a < b) && !(m >= c)` negated float tests), CameraBinocular::move 79 -> 0 (two-arm clamps cross-jumped, zoom clamp as a value, PlRef(pPL)->mat, orientation call after the mode block), CameraScope::move 91 -> 0 (Key.on & 0x10, angle_min/angle_max clamp, worldMat, `u8 c = sct--`, FRef(ytime)/FRef(xtime), Joy word-0 read, block-scoped cModel* pl), IdBinocular::move 271 -> 0 (getColumn(cam->mat, 2, &dir); `cnt` crosses no call: `unitPtr(cnt + 1)` + `cnt++` after the stores, cse makes the increment a copy of the call-crossing temp, so `li cnt,0/1` are anchored below the unitPtr calls and cnt takes the temp's r30 by copy preference; `i = cnt` for the while loop and the digit loop, block-scoped `k` for the unit loops; `MessageControl* mc = &cMes` after setLayout so `&mc->mes[1]` stays `addi 240` off a pseudo; FRef on every read of ratio/m/n with `u->v0 = FRef(ratio)` before `u->v1 = 1.0f`; scr2.x not scr3.x)

## `game/cam_qfps.cpp`

- move: the frustum copy loop counts with `j` (one allocno with the Draw_line3d loop's j: crosses the PSMTXMultVec calls -> callee-saved r29, sched2 hoists `j = nj` above the call), tail stores roll, fovy, reset last; setAreaData(CameraCut*): `p = &ofs[i][j]` before the flags test (cse rewrites the first store to `stwx r3,r28` = j-giv + outer i*132 giv), `for (j..; j++, k++)`; zero code

## `game/card.cpp`

- 67/67, pure C: saveMain (case 6 `step = 0` zero shares the `(int) fileNo` shift-count local, so its li trails the rotlw, takes r0 and the stb cross-jumps into case 1's copy); errorDisp (`mesNo = K; cardcheck = 0;` in -0x204/-0x206 so cardcheck's zero is the switch index's, which then outranks mesNo for r31; `attr = 0` inside both -0x203 arms so jump1 cannot hoist `mesNo = 1`)

## `game/cloth.cpp`

- collision / model helper units (atari, cloth, ik, mirror, pendulum ...)

## `game/datactrl.cpp`

- dispDebug: one function-scope x1 shared with the over block (conflict union -> r5)

## `game/db_cam.cpp`

- 13/13: debugCamera::menu 2 -> 0. The campos copy as three named words (union word view, `*(u32*)((u32)d0 + k)` stores = memcpy's flagless MEMs) plus two tagged codeless anchors: `asm("" : "=&r"(t) : "r"(wy), "f"(0.0f))` after the copy gives the y load its 14th sched2 dependent (the sched2 y/z tie is priority 29/29, LUID z-first from the reg-weight sched1 order; dependents break it before LUID), the `"f"` input makes it ready at t14 behind the roll constant's `lfs` so no local-alloc life moves, `=&r` stops the wy/t tie; `asm("" : "=m"(ProjType) : "r"(t))` after FSet keeps it alive to sched2 (placed there: a slot between the up stores and the FSet pG reload breaks the fake-death overlap that gives the up `addi` r11)

## `game/db_menu.cpp`

- move: GetGameTime declared with its real `u32` return (main_sub.h) -- the call then SETS r3, so the `addi r3,&h` argument move has one dependent (the call) instead of two (call + the next `lha r3`), ranks below `addi r4/r5` in sched1 and issues after the `subf`; zero code

## `game/dbmodule.cpp`

- DrawObjWireframe: do{..}while(1) command loop (no rotation), ISet(DB_poly_num), `*pidx++ =` idx stores, `idx[2] = idx[1]; pidx = &idx[1];`, one `s16* v`, own `u32 m2` for the strip emit loop

## `game/debug.cpp`

- 11/11: processBarDisp 25 -> 0. C: the bar width `s16 w;` declared above tile 1 and assigned there (the shared `5`'s pseudo precedes the `4`'s on the global-alloc tie -> r20/r19); the fourth bar's base is x0 reused as the max (if/else spelling, r28 in pass 0 after x2). Tagged stand-in: a dead `if (proc_tick_idx == 0) x0 = 0;` at the loop body top (+4 real insns at loop pass 1 so the "%5.0f %s" high misses the 47 threshold and is hoisted in pass 2, after the giv inits)

## `game/dvd.cpp`

- 51/55 functions, all section sizes exact; readMain/Initialize/ErrCheck/DiscChange differ in register allocation only
- DiscChange 7 -> 0, zero code: the first `pSys->region` read through a reference (`SysRef`, no MEM_SCALAR_P) is gated by all four game[] template stores in sched2, so the copy issues in template order 0,8,c,4

## `game/emBar.cpp`

- do-while around emBarSetBreak: 7th weighted em ref (em r31, p r30)

## `game/emBarred.cpp`

- SetEmBarred: `em->hpMax = em->hp = 1000` repeated in every YarareInitCube arm so case 4 does not end in the call (flow `use 0` nop) and all arms cross-jump into its tail

## `game/em_cloth.cpp`

- Em18ClothSet 26 -> 0, pure C (the four codeless asms removed): the `if (a)` arm repeats `c->x40 = 0.1f; c->x44 = 4;` -- real uses for flow/sched1/regalloc (0.1 and 4 live across the branch: f11, callee-saved r28; the store weights and the 0.1 lfs priority follow), deleted by reload_cse_regs as no-op stores (reload_cse_noop_set_p: the MEM already holds the register) before sched2

## `game/em_set.cpp`

- 12/12, pure C, no asm/tags (the EM_SET_WORK_K asm pool pairs removed): the EmSetWork body is a MACRO in EmSetFromList2/EmSetEvent (a pool constant expanded in the caller keeps RTX_UNCHANGING_P; integrate.c drops it on an inlined body's MEMs, so the inline's `lfs` loads carried store dependences), and `em->x374 = 1.0e16f` is a caller statement AFTER the plDist2 inline (inside it the constant load wins the sched1 tie against `dz*dz` by the last-scheduled-insn class and local-alloc's fake_birth then denies it f12); EmSetFromList keeps the inline (macro: 11 words)

## `game/em_sub.cpp`

- 51/51, pure C: EmCatchMotionMove (one `tmp` for the rot.y load and the turn step: two deaths -> global.c f13, no local-alloc ties into ry / rate); RandomItemCk (RandomHandgunAmmo writes the caller's num through a `u32&` -> one global pseudo whose preference is the first-dice chain's r29; `*= 5` as a separate statement)

## `game/embarrel.cpp`

- object units whose cAtariInfo::init argument order needed atari_init.h

## `game/embox.cpp`

- enemy closer: emBoxDmCk's second switch lays its SetBreak(0) arms out before the 7/8/0x21 if/else

## `game/emdoor.cpp`

- enemy closer: ckObj's `&EmMgr` hoist (block-scoped manager pointer + two-statement work address)

## `game/emmine.cpp`

- R1_Shot/R1_ShotArrow: the no-info block's Effect*Delete tail written out (jump2 cross-jumps it into DELETE_EFFECT), so at sched1 the SndCall block runs through three more calls and its r7/r8 argument moves (never re-set) collect an anti-dependence from every later call: depend count r7/r8 4 > r4/r5/r6 3 > r3 2 gives the target order r7, r8, r5, r4, r6, r3; zero code

## `game/emobj.cpp`

- enemy closer: setYarare narrow re-extension (int copies laundered through empty asms, flag pinned to r6), SatMgrCreateF floats-first alias, dead pool-only static (STRIP_UNUSED)

## `game/emrock.cpp`

- emRockDropCamMove: the `up` stores written BEFORE `len` -- after the six len loads the up.x store is the 34th memory insn of the block and sched1's 32-entry pending-list flush lands on it (every later memory insn anti-depends on it, the three pool highs swap r27..r29); zero code

## `game/emshield.cpp`

- setFall: DFmode `register f64 asm("fr1")` read in a "=m" asm keeps f1 live past the parameter copy (#8: the copy ranks as weight +1)

## `game/emswitch.cpp`

- object units whose cAtariInfo::init argument order needed atari_init.h

## `game/emwep.cpp`

- setCloth: statement order num, zeros, tables, owner, x38, floats (x48 before x50), `x54 = 0` LAST (the base register's death rides the zero store, so no float store is weight -1 and the 0.0 pseudo dies late enough for 0.6 to outrank it in local-alloc); emWepEscapeCamMove: the fovy store through `*(f32*)(u8*)&` (no MEM_IN_STRUCT_P: may alias the `pPL` load, whose chain then ranks `mr r29,r3; addi r31` above the pool `lis`es); zero code

## `game/eprintf.cpp`

- eprintf: `dst = mess_keep_ptr` read inside the slot-search loop with a single early return (gcse PRE copy instead of cse), loop test through a cast so the table pointer is reloaded per iteration, `s16 x = lo; x |= hi << 8` byte assembly, s16 coordinate copies declared after GXBegin

## `game/esp.cpp`

- operator new: loop3 as a `for` (cse_around_loop copy) with a do-while(0) around `PushEsp; goto found`

## `game/esp02.cpp`

- esp02Trans_sub: `&d` in the calls and the inlined VECNormalize (no `pd` copy: one fp+232 pseudo, spilled, its uses rewritten frame-direct), `org.x/y/z = 0.0f` as three statements, `nz = 1.0f - nz` in place, `u32 i` (unsigned `addic.; beq` exit); zero code

## `game/esp04.cpp`

- move10: y-loop step pinned to fr0 (#17) + a "=m" keep-alive of y after the loop (#13): the `v = y` copy is issued after the hoisted step/bound copies and the loop bound cannot take y's f12

## `game/esp08.cpp`

- Esp08_Trans/TransShimmer 143/143 -> 0 (shared ESP08_TILES macro): codeless `+f` launders on the four first-tile copies (cse1 promotes each copy to canonical and rewrites the mask quad, gcse/cse2 redo it; COMPILER-DIFF: first-tile copy canon), asm-emitted `fdivs` for the first tile's du/dv (the C divides block the fpu for 17 cycles and push the cu+du/cv+dv adds past the second y0 store, so reload inherits the y0 reload the original re-does; COMPILER-DIFF: asm-emitted fdivs), one `=m` keep-alive of y/st1 after the mask quad (f24/f21 global-alloc order), and zero-code statement order u0/dv/v0/du in the double loop's (j == numX-1, i != numY-1) arm (local-alloc f8/f9); .sdata padded to 8 (`asm(".section .sdata; .balign 8")`, the split object is 0x10)

## `game/esp09.cpp`

- PolyTrans: `pn = &w->pts[idx]` giv read by the Subtract argument, `p = pn` as an asm-emitted `mr` (cse canonical), idx-- between the p0/pp copies; two codeless "=m" asms with dead in-loop mentions rank p (17 refs) and pp (14 refs) above esp in global-alloc (candidate: global-alloc priority)

## `game/esp0a.cpp`

- Esp0a_Trans/SetFreeWork: the four polymorphic `*p = *q` copies written plainly (pure C since the shipped-build-temp-flags compiler patch, 2026-09-11), `colRSpd..scaleCnt` as separate statements (a reload of e.p per store), `int ot = 8` local before the parent test (callee-saved r29 across PSMTXMultVec), pEffParentWorld read through an inline reference

## `game/esp0e.cpp`

- Esp0e_Trans: plain polymorphic `*p = *esp` (pure C since the shipped-build-temp-flags compiler patch, 2026-09-11); stores `id, pModel, partsNo, life`

## `game/esp0f.cpp`

- esp closer: sprite corner idiom in Shimmer/Nega, EspSeqSet real parameter order (f32 before out)

## `game/esp12.cpp`

- Esp12_Trans: `u32 magic = 0x43300000` in the CommonStateSet block + a "=m" keep-alive after GXBegin (#13: the conversion constant is a 3-ref callee-saved pseudo hoisted to the block top), the Esp16_Trans zero recipe (fr12 pin + dead three-load test), nrm zero stores as three statements, `r*sizeY + (1-r)*sizeX`, `t += tstep` between the two vertices

## `game/esp16.cpp`

- Esp16_Trans: dead three-load test after the 0.0 load splits the block at sched (COMPILER-DIFF candidate, sched block split) + the #13 fr12 pin; keep-alive asm dropped

## `game/esp18.cpp`

- Esp18_Trans 14 -> 0: `u32 i` declared before the `oy == zero` test (pseudo 237: gcse's PRE pseudos are numbered in hash-bucket order, so `i + 1` follows fp+0xc0 and the spill slots are 0x234/0x238 as in the target) + one codeless `asm("" : "=m"(inv[0][0]))` in the loop (the hoisted 1.0/0.5 REG_EQUIV constants tie at priority 171, older 0.5 coloured first; COMPILER-DIFF: candidate (loop.c insn_count))

## `game/esp45.cpp`

- Esp45_HideCheck: dead `if (w->flags == 99) ox = oy;` at the loop body end (+3 real insns: high(Screen) misses loop pass 1's threshold and is hoisted in pass 2, after the giv init)

## `game/esp_app.cpp`

- EspDrawLaserLine: 0.8f as a named .rodata word + asm lis/lfs with hi pinned r11 and `li r8,5` asm-chained (#13); EffAreaUpdate: codeless "=m" asm as a sched2 issue-slot filler

## `game/esp_efm.cpp`

- EfmSetObj04: parent/parentSerial stored through a u32* (register-address store invalidates x79 in cse1)

## `game/esp_sub.cpp`

- esp closer: sprite corner idiom in Shimmer/Nega, EspSeqSet real parameter order (f32 before out)

## `game/espgen02.cpp`

- espgen02_Update: colR pinned to f24, copies between the bScale/bSpd zero stores (#17)

## `game/espgen10.cpp`

- EspgenDataSet: EspEvModList high/low as pinned r9/r11 asm insns + pinned r9 index (#13: REG_EQUIV high and mem never allocated in the original) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `game/espgen44.cpp`

- Filter05SetParam per-call-site argument-order aliases

## `game/espgen45.cpp`

- 20/20: Espgen45_Move00 42 -> 0 with the same loop-B shape (p-field anchors `"=m"(p->damp)` also close 42 but their three p address refs push p from r28 to r31 in 45's global order; the frame anchors on `v` add no p refs). Remaining tags in Move00: loop-B asm set/use, the jq pin, the three fillers

## `game/event.cpp`

- DelEvt: FadeSetW written out (`u32* c`, `c[1]` keeps P tied to r4), `int zero; zero = 0;` set before `if (fade)` (#13 single-use zero, update_equiv_regs moves the li to the store -> r0)

## `game/examine.cpp`

- item examine: single-assignment float locals for the camera setup (dist/r/y/z each die once, so local-alloc orders the FPRs by refs/length), `-y` stores with target.y first

## `game/exception.cpp`

- ErrorHandler: 15 dead `f32 lcN = K;` pool constants shift the string labels to .LC79/.LC80 so the gcse PRE pseudo order (hash-bucket) gives DSISR r16 / symbol_err_tbl r15 / CALL STACK r14

## `game/file_app.cpp`

- system units: file_lock reads pUser_name directly in each arm (a `path =` reassignment gives a phantom r31 save); pl_sub: `cPlayer* pl = pPL` before a switch, `goto` to a shared `return 0`, `const f32` limit across a call, guarded do/while list walks testing `next`

## `game/filter06.cpp`

- cParticle06::move: alphaBase pinned to r11 (#17)

## `game/foot_shadow.cpp`

- collision / model helper units (atari, cloth, ik, mirror, pendulum ...)

## `game/foot_shadow_tbl.cpp`

- collision / model helper units (atari, cloth, ik, mirror, pendulum ...)

## `game/game.cpp`

- game task: cGameSave shared-temp fixups, GamePointInit/GameAddPoint switch shapes, fadeSetG

## `game/id_sys.cpp`

- set: the twelve `ofs -> pointer` diamonds store in BOTH arms (`if (a) u->x = (T)(a + data); else u->x = 0;`): jump2 cross-jumps the stores so the join block starts at `lwz pG` and the `c = 0` li (prio 2) ranks below it; zero code

## `game/id_tex.cpp`

- enemy closer: id_tex (IdTexSet's laundered u8 argument, IdTexDataLoad's integer table base)

## `game/ik.cpp`

- collision / model helper units (atari, cloth, ik, mirror, pendulum ...)

## `game/item.cpp`

- set_stage2: LV_SET macro sets the EX nibble first (the all-zero mask chain then folds in combine, whose dead loads leave the USE insns that make `mr r3,this` the loop-note barrier); init: `li r3,32` as an asm-li with a dying input (#13); trigger: r9 pin + launder for the u16 mask (#2)

## `game/lightPath.cpp`

- the `||` return-0 test written as `goto ng` so no label precedes the surviving `return 1` (pl_class isKamae); a `do {} while (0)` around the last `pCur` store doubles that `this` ref's global-alloc weight (lightPath movePath: this r9, pCur r11)

## `game/main_mem.cpp`

- MemCheckUsedHeap 143 -> 0, zero code: y0/y1 and `int x = 498` function-scope (x = REG_EQUIV constant rematerialised by reload before each `sth x0`, so the code constant's r0/r11 is reused), datactrl's tile store order (code, x0, y0, ...; r, g, b), `mt = &tile[0]` before the end-marker conversion, `hd = HeapHead + handle` for the cell loop

## `game/main_sub.cpp`

- closer: plain-block HALT (li r4,0 before the string in DLL_Link/DLL_Unlink), lbl_ names for the two unreferenced .sdata ints, chained/zero store orders

## `game/mercenaries.cpp`

- GetSaveWork: `u32* tbl = SysRef(pSys)->x10; w = tbl[i]` -- the pointer local carries REG_POINTER, so regclass makes it the base of the `tbl[i]` address and i*4 the GENERAL index (r0, dying at the load); the rank-pointer giv init then issues second in sched2 (anti-dependence on the `clrlwi r0`) and takes r12; zero code

## `game/math_sub.cpp`

- SINF/COSF/RSQRT: paired-single / `frsqrte` kernels, vendor inline asm (GCC 2.95 has no intrinsics for them).
- LIMIT_ANGLE: also vendor inline asm, kept as such. The C spelling `if (!(x < max)) do x -= step;
  while (!(x < max)); else if (x < min) do x += step; while (x < min);` reproduces the compare/branch
  polarity (`blt`/`bge`, no `cror`) but jump2 turns the `b end` of the first arm into `blr` and the
  second arm's `bge end` into `bgelr` (jump.c inserts a RETURN before the epilogue of a leaf routine
  once reload has run, then redirects every jump to the return label): the target's `b 0x1c70` to a
  lone `blr` is a jump inside one asm statement, not something this compiler emits from C.

## `game/merchant.cpp`

- sellPrice: the bullet PriceEntry reuses `p` (2-set pseudo: no birthing boost on the `mr.`, so sched1 puts it after the two pool `lis` and local-alloc gives the highs r11/r10)

## `game/mes.cpp`

- Message::move/WidthCk: a codeless asm between `code = getCharCode()` and its `== 0` test (input-only use in move, `+r` launder in WidthCk) stops combine from fusing the call-result copy and the compare into `mr.` (COMPILER-DIFF candidate)

## `game/mirror.cpp`

- collision / model helper units (atari, cloth, ik, mirror, pendulum ...)

## `game/model.cpp`

- drawBoundingBox: counted `for (j < 8) PSVECAdd(&v[j], ..)` loop (giv init after the gcse insertions, biv-eliminated `cmplw; ble`) instead of the do-while pointer loop; zero code

## `game/motion.cpp`

- MTX_COPY: dst pointer first, `int i_ = 2` between the two pointers, `for (; i_ != -1; i_--)`, `d_++` before `s_++`; tbl end pointer as two statements; `u32 zero` with a dead-use anchor (COMPILER-DIFF) and a memory anchor in MotionSetCore; nearZero(1.0f - v) inline with `f32 one` loaded first

## `game/obj00.cpp`

- FallMove: codeless call-crossing `junk` pseudo (asm def before the hit loop, "=m" use in the fallSpd loop) ranks between `end` and the hoisted `sePlayed = 1` constant and takes r24, so the constant gets r23 (candidate #17); four dead `i = K` sets keep the gcse bucket count

## `game/obj02.cpp`

- misc object units: dead-stripped out-of-line inits whose pools survive (obj03 init), .sdata pads

## `game/obj03.cpp`

- misc object units: dead-stripped out-of-line inits whose pools survive (obj03 init), .sdata pads

## `game/obj12.cpp`

- misc object units: dead-stripped out-of-line inits whose pools survive (obj03 init), .sdata pads

## `game/obj1b.cpp`

- obj1bHitCk: `no = partsNo ? partsNo - 1 : 0` ends cse's ebb at the select's join, so the later `&obj->pos` occurrences are PRE'd into the post-getPartsPtr copy

## `game/objMissile.cpp`

- object units whose cAtariInfo::init argument order needed atari_init.h

## `game/objPillar.cpp`

- misc object units: dead-stripped out-of-line inits whose pools survive (obj03 init), .sdata pads

## `game/objRobo.cpp`

- R0WalkBridge (zero code): the first flag test reads the word into a user variable (no cse jump threading past the second test, so the 0x80000000 `lis` stay per block and are not combined/hoisted), `hp` plain pointer for hitCnt (pG reloaded), `hp++, i++`, FRef(RoboFallSpdY); TaskSwitchFront/Back: `range = to` in the for-init, `range2 = from - to` in the up loop, no `base` copy, plus one tagged fr28 pin on `to` (#17) that keeps the for-init copy out of gcse cprop

## `game/objTrolley.cpp`

- object units whose cAtariInfo::init argument order needed atari_init.h

## `game/objWep.cpp`

- drawPoint: first `esp` reload pinned to r11 (#17)

## `game/objYagura.cpp`

- object units whose cAtariInfo::init argument order needed atari_init.h

## `game/option.cpp`

- 20/20: retry_load_menu zero code (`Cckpt.roomInit(); Cckpt.move(); Cockpit* ck = &Cckpt;` keeps the Cckpt@ha high alive across the first call so local-alloc cannot tie ck to it); controller_menu: own `j` counter for loop 2, `BitOn(pSys->flags, ..)` before VibSet (store not disjoint from the vib_time/vib_level scalars), `j == o->sub`, one codeless `asm("" : : "r"(sel))` after loop 2 (COMPILER-DIFF candidate: sel 29/338 must outrank o 42/600 in global.c)

## `game/pad.cpp`

- PadRead: `register int dead asm("r16")` set by a volatile asm `li` (#17/#13): the hoist keeps r17, the li is the block's first insn — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `game/pendulum.cpp`

- PenClothMove/Move2/Move3: the final-loop AtCk diamond needs the jump2 cross-jump the other way round (AtCk tails merged, Border tails kept) -- codeless `asm("" : "+r"(hit))` anchors after each penClothAtCk call and after the if-arm's join, one macro line so rtx_renumbered_equal_p matches them; Move loop 2: `asm("" : "+f"(ang))` launder after `ang = 1.0f` keeps the `+ 1.0f` pool loads (loop.c combines the two into the preheader's fresh-high `lis r9; lfs f30`), and r5/r3/r4 argument pins + `asm("mr")` copy into `register Vec* pv asm("r28")` for the `addi r5,r1,0x38; mr r28,r5` PSVECSubtract shape (#3 frame-address PRE); tagged COMPILER-DIFF

## `game/pl_class.cpp`

- the `||` return-0 test written as `goto ng` so no label precedes the surviving `return 1` (pl_class isKamae); a `do {} while (0)` around the last `pCur` store doubles that `this` ref's global-alloc weight (lightPath movePath: this r9, pCur r11)

## `game/pl_debug.cpp`

- DrawGage: opaque `fadds len,fx,len` (expand_binop puts the destination operand first; tagged candidate)

## `game/pl_npc.cpp`

- partner character (pl_npc): const f32 locals for pool order, per-value switch bodies, dead `farCheck` inline whose pool word survives the strip (STRIP_UNUSED)

## `game/pl_sub.cpp`

- system units: file_lock reads pUser_name directly in each arm (a `path =` reassignment gives a phantom r31 save); pl_sub: `cPlayer* pl = pPL` before a switch, `goto` to a shared `return 0`, `const f32` limit across a call, guarded do/while list walks testing `next`

## `game/pl_wep.cpp`

- 29/29: PlWepHitCheck2 180 -> 0, pure C: `case 7:` stacked on `default:` (block LCM inserts the second switch's compare at the end of the left-root block) and the 4/8/0xC body placed after 5/6 (leaf layout = source order)

## `game/puzzle.cpp`

- 49/49: shape 1 -> 0 (pass 5): px enters the QImode multiplies as a one-byte `struct { s8 v; } w` -- a RECORD_TYPE local is not PROMOTE_MODEd, so `w.v` expands to an unpromoted REG:QI and expand_binop's "op1 REG, op0 not" commutative swap no longer fires for byte 2 of rot (the HImode extraction copied into a fresh QI REG); cse folds `(subreg:SI w)` to px, no instruction added; COMPILER-DIFF). movePiece/PutInCase/shape join per passes 1-4.

## `game/read.cpp`

- readEmData: do-while around the dll else arm (m 15 weighted refs > newSize)

## `game/rnd.cpp`

- `m = (n << 16) >> 16` keeps m out of n's cse class (the `mr r0,r9` copy)

## `game/room_jmp.cpp`

- roomJumpMove: nested s8 ternary through an s8& setter (QI temp -> jump1 hoists the 0 -> conflicts with r3)

## `game/roomdata.cpp`

- init: the total loop counts with `stage` (its `stage + 1` gets the lower gcse expression index, so the PRE'd increment is inserted before the hoisted `(u8) stage` and ofs/nx take r27/r26); linkRelData: `rel_no` read directly in the compare and the store (HImode load + one shared `clrlwi` for the compare and the DvdRead argument); zero code

## `game/route_ck.cpp`

- Draw_rtp: the link loop's test refreshes a `GlobalWork* g` local, `i < ((RtpData*)(g = pG)->pRoomRtp)->nPoint` -- the pG value is ONE pseudo (g is referenced outside the copied exit test, so duplicate_loop_exit_test keeps it) whose PRE'd copies survive as `mr r11,r5` at the entry and the latch, and the body's `rtpData()` reload stays; zero code

## `game/sce_at.cpp`

- sceAtGetItem 77 -> 0: `register u32 money asm("r29")` in case 8 (COMPILER-DIFF: 13, the it/ItemMgr-high/money rotation settles to r31/r30/r29), `int sel;` without initializer (cancel's zero is then the newest for `swep_flag = 0`, cancel lives from the top and ranks below sel: r29/r25), `put = 1` after `ItemMgr.use(&tmp)` (same as NoModel)

## `game/sce_com.cpp`

- 33/33: SceElevator 232 -> 0 (r225.cpp's SceElevator_r225 shape: SetPosXYZ/FadeSetRGBA inline helpers sharing frame slot 8, the goto-entered up loop, the down loop with its tail inside; residue closed by `register Vec* jp asm("r25")` for &d->jumpPos -- `done` (10 refs / 424 = 106 x4 from two `done = 0` REG_EQUIV doublings, 707) sorts below gcse's &d->pos copy (9/380 = 710) while the target has done r25 / copy r24; COMPILER-DIFF), OpenBoxMain 182 -> 0 zero code (case 0x17 laid out after case 0 in both switches; a per-loop `int i` so the two-frame waits' counters are short pseudos allocated first: r31 before type r30 / o1 r29, id2 and the 30-frame counter reuse r31), SceSetItemEvent 17 -> 0 (the new'd entry is a second variable `ne`; `asm("li %0,0" : "=r"(j))` keeps j undoubled (11 not 22: 61818 > e+6 48000) and a pseudo so expand_mult's `copy + j` stays `add` instead of the pin's `slwi`; COMPILER-DIFF)

## `game/sce_sys.cpp`

- ScenarioRoomInit: store order only -- the six byte zeros first (cse makes their QImode pseudo before any SImode zero exists; a later word zero's low part would replace it), eventCancel last of the six and pause last of the words (sched1 issues each group's dying store first), the byte group's `li`/stores ranked last by sched2's anti-dependence on the pG load's r9; zero code, the #13 asms removed

## `game/scroll.cpp`

- scroll: index-first integer table address in SmdClear (the store aliases the scalar table pointer), format strings in `const char*` locals so jump.c does not hoist the early `return NULL`s, `goto ok` around the shared NULL return, do/while list walk testing `next`, `u32 base` for the bin table, `nTpl = 0` before the getWorkPtr call; SmdGetIdNumPtr is dead-stripped (STRIP_UNUSED)

## `game/shadow.cpp`

- make_comn_fit/parallel_light: `register u32 k asm("r11")` defined by a codeless asm fed by the 1.0 pool value and killed by a `"=m"(pos.x)` asm -- r11 is live between the 1.0 `lfs` and the conversion's `lfd`, so the fpmem loadaddr qty (ranked above the 1.0 high in local-alloc) cannot take r11 and the 1.0 high does (COMPILER-DIFF: 13)

## `game/snd.cpp`

- SndCall RefU32 flags read (param stores rank above lwz pG), SndSetReverb one `p` for both arms, SndRoomBgmStart nested do-while weights, sndVolCalcSub dead `dist > vol` test (r -> f2)

## `game/sscrn.cpp`

- SubScreenExit: `u32 clear = 0` before cMes.roomInit() feeds a local FadeSetBlackOut inline (the zero stays a loop-body pseudo, not combined with the `type`/`flags` zero and hoisted), `type = 0; flags = 0` separate, BitSet/BitOff reference stores, `area_no != -1` polarity; OpeSetOpenTerm: codeless asm past z's death (tie, x 216 -> 217); the 12-byte .text pad is lib/ppcdown.s's `.balign 32`

## `game/t_bugcheck.cpp`

- menuLife 4 -> 0: the "LIFE" high is a pinned asm `lis r17` in the preheader (LUID before the eight PRE'd string highs) plus an asm `addi` for the eprintf argument (COMPILER-DIFF: #13 asm pool constant); the dead `lv = 3` bucket knob is gone, the nine dead pool constants and the PlKaiou anchor stay

## `game/t_flag.cpp`

- move: `cur & 0xF` pinned to r11 (untied add) and `cur >> 6` pinned to r0 (#17)

## `game/t_option.cpp`

- tp_pl_flag case 4: codeless asms reading the num() result in r3 (uninitialised `register int asm("r3")`) give the arms' `addi r3,r30,ItemMgr@l` an anti-dependence so the `li` constants issue first (candidate #1)

## `game/texture.cpp`

- DataLoad: ofsId/ofsTpl temporaries pinned to r9/r11 (local-alloc fake-lifetime tie; same schedule)

## `game/title.cpp`

- titleSub: every fade through FadeSetW (the colour pair is the inline's own BLKmode local at 32/36; in the `flags_54 & 0x40000000` arm cse rewrites the start address to the frame pseudo, `mr r4,r29` PRE'd, while `&col.end` stays a hard-reg-dest `addi r5,r1,36`); titleDebugMenu: one `int no` for case 3's checkRoomNo result and case 5's room (crosses the getPointNum calls -> r31 for both, the dbgPoint temp r30), the s8-parameter calls through int-view aliases (#4)

## `game/trans.cpp`

- transfer units (lighting, ordering table)

## `game/trans_lit.cpp`

- transfer units (lighting, ordering table)

## `game/trans_ot.cpp`

- transfer units (lighting, ordering table)

## `game/vfprintf.c`

- fftoa: do-while notes around `u.d = value` and the lo/hi word reads

## `game/view.cpp`

- initPerspective: ONE frustum pointer `b` for both halves (the halves' `&b->point/normal[k]` are one gcse expression set: the second half's preheader addi order and the first half's single `&point[4]` PRE follow), `ViewSphere* s = &sphere` for the centre/radius stores (cse's find_best_addr rewrites `(mem s)` to `892(this)`, the rest stay `s`-based, s r18), `/ (2.0f * det)` inline (a fresh call-anchored pseudo), zfar stored before znear; two dead statics carry the .rodata 0x48..0x7f pools (STRIP_UNUSED); zero code

## `lib/FSasync.c`

- SN libsn FSasync: volatile transfer state (reloads), `for (;;) { cnt--; if (cnt == -1) break; }` loops (shared @ha register), 32-byte aligned DMA result struct, -fno-common .bss order

## `lib/__start.c`

- hand-written libsn/crt assembly (src/lib/*.s, see configure.py ASM_UNITS): local branch targets are resolved by the assembler, so objdiff shows ARG_MISMATCH on them; the linked bytes are identical

## `lib/_eh.c`

- functions match; the 16 .bss bytes are the remainders of the dead-stripped runtime's statics (three _register_malloc hook words appended)

## `lib/adx_baif.c`

- CRI pass 10: 5/6, AIFF_GetInfo 91w (M1: FORM/size words share the loop registers); ExecOneAiff16 fixed by the `Uint16` swap temporary
- 6/6, pure C: AIFF_GetInfo 170 -> 0. `Sint32 ckid/type` compared with int constants are read as int-typed indirections of long objects, which the frontend never propagates into (the kept header ckid = `mr r27,r30`); `end = p + cksz - 4`; the *nch/*bps reads spelled `(p[0] & 0xFF) | ((p[1] & 0xFFFF) << 8)` (clrlslwi 16,8 base + rlwimi 0,24,31); exp/mant `Uint16` own locals with `SWAP16((Uint16)(lo | hi << 8))` (the cast breaks the frontend CSE, the 16-bit value is a backend temp coloured before the byte temps)

## `lib/adx_bau.c`

- CRI pass 10: ADXB_ExecOneAu16 2ch swap through a `Uint16 x` temporary `(x << 8) | (x >> 8)` gives the unroller's 15th copy as `extrwi` (the "M6" was a source shape)

## `lib/adx_bsc.c`

- CRI pass 18b: EvokeDecode arms `pcm = pcmbuf; pcm += wr_pos` (pcmbuf is the in-place add destination = r6); `asm { add ofst, x70, ofst }` operand-order pins in ExecOneAdx/EvokeDecode (COMPILER-DIFF: M1) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/adx_bwav.c`

- ADX_DecodeInfoWav: dsize swap before *hdrlen/*x0c (the -1 is stored mid-chain, so the chain lands in r3 and is copied), `4 + i + (Sint32)buf` keeps add/lwz 4()

## `lib/adx_dcd.c`

- ADX_GetCoefficient: `#pragma pool_data off` before the function (COMPILER-DIFF: M2 - the original does not count the function's own float literals towards the pool threshold; CRI pass 9)

## `lib/adx_dcd5.c`

- CRI pass 9: ADX_DecodeSte4AsSte 118w / Ste4AsMono 167w / Mono4 39w (M1 register ranking only: the original keeps smul in r0 / i in r10 / c1,c2 extended in place; the history locals declared first fixed the AdxQtbl address hoist)
- 4/4: ADX_DecodeSte4AsMono 2 -> 0 (`nfrm / 2` add temp r0 vs r12 + the two pool `lis`). Tags: M1 (neighbour pin `asm { mr r6, l1 }` in both Ste/Mono), M1 (kept parameter copies `mr r6, c1/c2`), M1 (neighbour copy `asm { mr x, t }`, Ste), M1 (dead conditional in the l1 clamp, Mono), M1 (dead consumers, entry-block order, Mono: `register x = c2 + *ps; y = (Sint32)scl + smul;` + `asm { mr r6, y } asm { mr r6, nblk } asm { mr r6, x }` — the B1 slot read of the address-taken addend is the CSE target of the loop's hoisted load (a backend temp, level r0) and is scheduled after the pool `lis` (base pair -> spill picks, magic r20 / table r21) and before the `srawi` (add temp adjacent to sadd -> r12); all dead defs deleted by the RA, size exact) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/adx_sjd.c`

- adxsjd_decode_prep: `asm { lwz r5, ck.len; mr len, r5 }` pins the post-call single-use length to r5 (hard-register asm pin, COMPILER-DIFF: M1) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/adx_sje.c`

- 17/17, pure C: adxsje_write_end_code 2 -> 0. adxsje_put16 stores through a write cursor `Sint16 *dp = ck.data; *dp++ = *src;`: a two-use pointer is kept by the frontend, so the ck.data load precedes the value lha in the pre-RA schedule input (a substituted single-use pointer evaluates the RHS first) and wins the equal-priority cycle-0 tie

## `lib/adx_stmc.c`

- CRI pass 10: adxstmf_create's search as `(Uint8 *)adxstmf_obj + ofst * sizeof(ADXSTM_OBJ)` with `ofst++` after the test (scaled-index IV stepped in the latch)

## `lib/adx_tlk.c`

- adx_tlk (CRI ADXT handle API): `-inline auto,deferred` (CRI_CFLAG_OVERRIDES) — .text is the reverse of the source, the public accessors are inlined into SetOutputMono/GetTime/DiscardSmpl/StartSj, the dead-stripped API functions are kept for their strings (codegen order), .bss reverse declaration

## `lib/adx_tsvr.c`

- 6/6: adxt_nlp_trap_entry 2 -> 0 (`lha r4` vs `r0`). Tagged M1 (rA use of the lha temp kept to the RA by a dead conditional): `register Sint32 t = ofst; z = 0; if (z != 0) { ofst1 = t + 4; } ofst1 += t;` — the frontend keeps the own-local constant out of the relational compare, so at RA time the arm's `addi ofst1,t,4` marks t's web no-r0 (physical r0 in its neighbour list -> r4 = the target's colour, z takes r0); the post-RA peephole folds `li; cmpi; bt` into the fall-through and deletes the arm, the join block (`cmpwi n1; lha ofst2v; add`) is scheduled as the target's; size exact

## `lib/ax_rna.c`

- CRI AX renderer: inlined public setters/getters, `sw` flag byte with |=/&= masks, separate loop/cur locals for the AXPBADDR fill, inlined axrna_exec_trans/axrna_exec_flash helpers

## `lib/builtin-delete.c`

- strings of SN's dead-stripped libstdc++ operator new/delete stubs (static bodies, LIBSN_UNITS strip)

## `lib/cftcoladj.c`

- `cbtbl[i] = i * v / j` (the i*v product is MWCC's strength-reduced IV, not a source local); last ramp loops on v with the start copied

## `lib/cftfx.c`

- 6/6, pure C: cnvDynamicYcc420plnToA256UserTable 2 -> 0. The two strides are own locals declared first (`Sint32 w4; Sint32 dskip;` = the level's two highest vids, r0/r3 before p4) with `w4 = ywidth * 4` a statement before yskip's (the setup slwi/add pre-RA tie is input order; a hoisted @temp is appended after the for-init); dskip in bytes (`/ 4 * 64`, byte-pointer add) so `d += dskip` is not folded into a hoisted `<< 6` @temp

## `lib/cftyp422_ppc.c`

- 8/8, pure C: CFT_Ycc420plnToY84C44 29 -> 0. `const CFT_YCC420PLN *src` (parameter loads above the frame stores); the setup's four signed divisions are chained through CA (srawi writes, addze reads: latency edges in statement order), so their statement order yskip, cnt, hblk, dskip is the schedule and `li ofs,8` issues first with r0 live across the temps; ywidth/yw3 reused for the chroma loop (cbwidth/4 and *3) and declared last = one node each, degree >= 29 at scan 1, coloured r9/r10 before the unroll-remainder copy (r11)

## `lib/cri_cvfs.c`

- CRI pass 13: 11/13, cvFsGetFileSize 63w / cvFsOpen 240w (-4 bytes) (the inlined cvfs_ResolveDev: pdev above tbl, tbl materialised after strlen); cvFsAddDev fixed by kept devname/getif copies + the inlined cvfs_AddDevTbl helper
- CRI pass 19b: 11/13, cvFsGetFileSize 63 -> 45w (`tbl = cvfs_tbl` assigned after the default-device block), cvFsOpen 148 -> 152w (-4 bytes); the inlined cvfs_ResolveDev's tbl still hoisted above the strlen
- 13/13: cvFsGetFileSize 14 -> 0 with a codeless M1 level-shifter pin of pdev to r11 (a register no value of the function takes): the extra physical neighbour keeps pdev in the Chaitin graph one iteration longer than tbl -> fname r29 > pdev r28 > tbl r27; a callee-saved pin reserves the register and shifts the ResolveDev locals (pass 35's 20w)

## `lib/crt0.c`

- data-only libsn crt0 half: the two 32-byte message buffers, the version words, `LinkFiddle = {__mod2i, 0}` (the code is lib/__start.s)

## `lib/crtbegin.c`

- .ctor/.dtor -1 list heads (`__CTOR_LIST__`/`__DTOR_LIST__` with section attributes)

## `lib/dct_ac.c`

- CRI pass 8 (pure-C revert): DCT_AcInit 37w (M2: pooled literals + `...bss.0` base for dctac_version_dummy, one more callee-saved register)
- CRI pass 19b: DCT_AcInit 7 -> 0: the inner-loop row/column pointers are own variables declared below the asm .bss pool base (the frontend's range-split copies outranked the base, r29 -> r31), `addi ip, bss, __ArenaHi@l` = the pool-relative `addi 0` with a relocation (a literal 0 becomes `mr` in the backend's constant propagation); ldscript aliases _savefpr_27/_restfpr_27 (COMPILER-DIFF: M2)

## `lib/dct_fsri.c`

- DCT_FsriTransCore: the paired-single kernel's FPRs (c1..c6 constants, 9 temporaries) and the B0TableOrg pointer as `register __vec2x32float__`/pointer variables instead of hard registers (the C paths then share f0/f7 and r5/r7; cnt declared before o); dctfsri_Idx static inline with `int`s, initSparseTbl under `#pragma opt_loop_invariants off` storing through an inline `const Float64 *` helper; B0TableOrg[0] = sqrt(2)

## `lib/eabi.c`

- hand-written libsn/crt assembly (src/lib/*.s, see configure.py ASM_UNITS): local branch targets are resolved by the assembler, so objdiff shows ARG_MISMATCH on them; the linked bytes are identical

## `lib/fileserver.c`

- hand-written libsn/crt assembly (src/lib/*.s, see configure.py ASM_UNITS): local branch targets are resolved by the assembler, so objdiff shows ARG_MISMATCH on them; the linked bytes are identical

## `lib/gcci.c`

- per-handle `static inline gcci_ExecOne(GCCI ci)` (over, nbyte, p) + the table loop written twice (gcci_ExecServer(tbl) for gcCiReqRd, gcCiExecServer's own loop): the counter ranks above the induction pointer as an inlined local and below it as an own local, the CANCELED `over` redefinition takes over's register; `sctlen * (over / sctlen)`; pure C, no pins

## `lib/lsc.c`

- EntryFileRange: raw previous id read first, then ent = GetWrEntry, then the wrap ternary (temporaries in place)

## `lib/mfci.c`

- mfCiReqRd: asm-defined `register` copy of the mfci parameter (coalesced into the prologue mr.) ranks it r29 above buf r28 (COMPILER-DIFF: M1) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/mps_dec.c`

- mpsdec_DecPackHd: reader init written out (`cur = p[0]; ... cur <<= pos;`), locals declared p, pos, cur, nxt

## `lib/mps_lib.c`

- 7/7, pure C: MPS_Create 2 -> 0. The syshd clear is three 8-iteration loops, not a nested i/j loop: the frontend unrolls the inner loop but leaves the outer counter to the backend, whose unroll keeps a dead `li i,0` in the store block; the RA deletes it, which clears the block's scheduled bit and lets the post-RA scheduler order `addi r0,..@l` before `li r4,-1` (the target = the pre-RA order)

## `lib/mpv_cdec.c`

- MPVCDEC_IntraBlocks: the six blocks cleared by six calls of a static inline helper storing 32 doubles through a `Float64 **cur` cursor (the second clear base `addi r8, mpv, 0x720` follows; CRI pass 9, shape from mk-deception)

## `lib/mpv_cmc.c`

- CRI pass 17b: the block tables addressed through an MPVCMC_OUTBLK struct pointer (`oi = ob->rt`: an addi off another addi is not folded by add-propagation), `work` local declared first

## `lib/mpv_dec.c`

- MPVDEC_END: ck.data as an asm-defined `register` local so q stays in r4 and the load takes r8 (COMPILER-DIFF: M1); `(Uint8)val` skip lengths (zero-code) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/mpv_frm.c`

- MPV_SkipFrmSj/MPV_DecodeFrmSj: asm-defined `register` copy of hn ranks mpv r31 above the other parameters and locals (COMPILER-DIFF: M1) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/mpv_hdec.c`

- MPV_DecodePicAtrSj: the skip helpers assign `ptr` INSIDE the bitpos expression (a nested assignment blocks the forward substitution of the single-use bitpos, so it stays a variable coloured after ptr and above the AnalyUd call) and take the byte pointer as `(Uint8 *)(ptr + 1) + n` (codegen emits `(ptr + n) + 4` with the sum a backend temp above the ck.data reload); the same `(Uint8 *)(ptr - 2) + n` replaced the pass-7 asm `lwz data` pins in MPVHDEC_FLUSH/DecSlice; pure C

## `lib/mpv_mc.c`

- CRI SWAR kernels pass 2: asm bodies replaced by pure C except OneRef1p (lfdux/lwzux: asm in the original); 4p 72w (count/stride r0 swap + loop registers), H2 436w (masks hoisted with opt_propagation off; the target's un-split loop variables), V2 73w (case 0 identical: average through mask variables + declaration order x0, w0, a0, x1, w1, a1; cases 1-3 register/mr residue), 1p 481w (the original's lfdux/lwzux update forms are not emitted from C by this compiler)
- 5/5 (OneRef1p keeps its asm body: lfdux/lwzux update forms are never emitted from C by this compiler). H2 34 -> 0: masks `Uint32 m2; Uint32 m1;` assigned m1 then m2 (web vids by declaration, lis temps by statement order), cases 2/3 inserts as asm-emitted `rlwimi` on `register` helper locals (COMPILER-DIFF: the target's order is the pre-RA schedule of a DAG with no leftover pcode; the or-pack's fused shift is a dead def until the RA and the intrinsic's K6 copy a node, and the scheduler model shows every extra node moving the order). V2 73 -> 0 pure C: `Uint32 x0 = 0, x1 = 0;` dead initialisers (range-split webs are numbered by each variable's FIRST definition and coloured in that order), the third load of each row kept as a variable by the pointer step right after it, the second pack written into it (`w2 = (w1 << 8) | w2` = the target's kept `mr r28, r31` in case 1 and in-place `srwi r28, r28, 8` in case 3)

## `lib/mpv_mcy.c`

- 5/5, pure C: MPVMC16_OneRef4p_TuneC 26 -> 0. The pixel-9 pair is loaded BEFORE the d[0]/d[1] stores: a `Uint8 *` local's load written after a store shares the store's alias class (pass 58) and the scheduler cannot lift it above the store, while the target issues both pixel-9 loads before the stores; with the loads first the block-1 schedule, the levels and every colour follow (the >100 block split after `b2 = s1[11]` is the target's, not "after the 9th sum"). 1p/H2/V2 as in SWAR passes 5/20/22

## `lib/mpv_umc.c`

- 16/16, pure C: mpvumc_OneReadMb 48 -> 0. The chroma half-vectors are the frontend's CSE @temps of `vx / 2` / `vy / 2`, first evaluated inside `cpos = ofs[0] + ((vx / 2) >> 1) + ((vy / 2) >> 1) * cpitch` (the target's six srawi = XER writers, serialised by the scheduler in statement order, run vx>>1, vy>>1, cvx, cvx>>1, cvy, cvy>>1; own locals `cvx = vx / 2; cvy = vy / 2;` give cvx, cvy, cvx>>1, cvy>>1); as @temps they are coloured before the own locals (cvx r28, cvy r7) and vx dies into fn_y's r25. `mbx8 = mbx * 8` a two-use own local declared last (`ofs[1]` uses `mbx8 * 2`, folded back to `slwi mbx,4`): coloured after mby/mbx -> r12, mby/mbx r10/r11

## `lib/mpv_vlc.c`

- mpv_vlc (CRI Sofdec VLC tables): `-inline auto,deferred` (CRI_CFLAG_OVERRIDES) — .text is the reverse of the source order and .bss is laid out in *reverse declaration order* (no first-reference placement under deferred), .rodata stays in declaration order; table fills are `for (i < n) *p++` loops (<= 8 iterations unrolled without guard, 9..32 with the `li 0; cmpwi n; bge` guard, more as 32-store mtctr loops); per-code-length groups are blocks with their own `k/i/v` (a shared `k` chains consecutive one-iteration loops); `Sint16 v` fills give the `extsh` of 16-loops (I tables, motion) while the P/B tables store the expression directly; the VLC area layout keeps the run/level start in a copy (`rl = p`) and derives the lower tables from it (r18 base)

## `lib/mpvabdec.c`

- two-word bit reader written out per case (`bitpos += n` before the coefficient stores, refill after); the non-intra first coefficient / skip / AC loop as three static inline helpers under `#pragma inline_max_size`; the escape look-ahead through a fresh `mpvabdec_EscapeCode()` value in NintraBlock/Dc11 and in place in IntraBlock; run/level tables read as `const Sint16 *`

## `lib/mwsfdcre.c`

- 10/10: mwsfcre_CreateSfd 115 -> 0 (the two dead `b`). mwsfcre_IsUseAdxt's `case 4:` arm FIRST in the switch with a tagged codeless `asm { mr mode, mode }` on the `register` parameter: the frontend folds an empty arm onto the default label and deletes every side-effect-free statement, so the original's case-4 block held a statement a backend pass (CSE / RA) deleted after layout; laid out between the compare tree and the FALSE arm, the emptied block keeps its `b T` at both inlined sites (an arm after the FALSE arm falls into T). mwPlyCalcWorkSfd keeps its pass-65 asm read

## `lib/mwsfdfrm.c`

- mwl_convFrmInfFromSFD: the ten frame-field copies declared LAST rank r31..r22 above the parameters; pptr declared before usrlen/usrptr (zero code, no pins)

## `lib/mwsfdply.c`

- MWSFPLY_SetFlowLimit: MWSFD_SetFlowLimit(mwply, 0.8 * n, n) takes a third argument (CRI pass 9 replaced the r5 pin, which was a missed parameter)

## `lib/mwsfdsfx.c`

- CnvFrmInfToSfx: parameter pins r27/r30/r31 + plane-1 loads as asm-defined register locals (COMPILER-DIFF: M1); tag strings named and declared before mwsftag_GetAinfFromSj for the .rodata order (COMPILER-DIFF: M3) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/mwsfdsvr.c`

- CRI pass 12: `void *obj` handlers with a kept MWPLY copy, function-scope sfd, the sleep loop as an inlined helper

## `lib/ppcdown.c`

- hand-written libsn/crt assembly (src/lib/*.s, see configure.py ASM_UNITS): local branch targets are resolved by the assembler, so objdiff shows ARG_MISMATCH on them; the linked bytes are identical

## `lib/proview.c`

- hand-written libsn/crt assembly (src/lib/*.s, see configure.py ASM_UNITS): local branch targets are resolved by the assembler, so objdiff shows ARG_MISMATCH on them; the linked bytes are identical

## `lib/reverb_std.c`

- AX FX standard reverb: `max_length << 2` in DLcreate decides `rv`'s callee-saved register

## `lib/rna_res.c`

- RNARES_Init: the hoisted 0x1000 and ptr+ofs named as asm-defined `register` locals -> volatiles in declaration order ofs, half, sum, ptr, res (COMPILER-DIFF: M1) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/sfd_adxt.c`

- 28/28: sfadxt_ExecServerSub 59 -> 0 with one M1 hard pin of the handle (`asm { mr r31, obj; mr sfd, r31 }`): the target colours sfd r31 above err (Transfer's coalesced @ret chain) r30 / len r29; the pass-35 helper split gave every other register — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/sfd_buf.c`

- CRI pass 22: 26/26, pure C, no pins. InitHn 447 -> 0: adr[9] stays a stack array (with 8 words the backend's array-register transform registerises it) written through a stepping pointer from one running `a` (`*p++ = a; a += prm->size[i]`), InitVfrm/InitAout take `Sint32 *size` read twice (`used` before the mode store, the field after the adr store), aout clears rsv[7] + 3 more words (rsv2), the uoch clear is a 3-channel loop, InitRing declares `used` before `mode`. DestroySj 20 -> 0 / SetSupplySj 36 -> 0 / RingAddRead 22 -> 0 / RingAddWrite 16 -> 0: the u.ring / sup pointer is a TWO-STEP address through an own `SFBUF_WORK *wk` local with its own uses -- add-propagation folds `addi wk` into `addi ring, wk, 0x10` but never propagates the addi it just rewrote, so `ring` stays a node (`addi rR, hn, 0x1318`); the AddRead/AddWrite bodies are `static inline` helpers (the first arm's `ret = 0` is a helper @temp: CSE'd into the entry zero, arm emptied, `bne body; b end`) with the locals declared in REVERSE of the target's colouring, the second sj a separate `sj2`, and `ring->dlm_pos` read inline in the compares (no `pos` local: a helper local outranks the frontend's ck.data CSE temps)

## `lib/sfd_cre.c`

- CRI pass 14: sfcre_AnalyMpv 15w (the `ofs + 1` backend temp is coloured after the b4/b7 byte variables in the target, r0, before them in ours, r4 in place of ofs) / AnalyAudio 43w / AnalyMps 28w (inlined-helper temporaries and callee-saved permutations)
- 6/6, pure C: sfcre_AnalyMpv 15 -> 0. `((b7 & 0xF0) >> 4) == 0` (mask then shift): peephole-forward folds the pair into one rlwinm and leaves the dead mask def in the block until the RA, one more scheduler node that pushes `addi ofs+1` past the compare (ofs r6 / b5 r6 no longer interfere); b7 declared between b4 and ofs and kept an own local by its second def `b7 &= 0xF` (no picrate_code local)

## `lib/sfd_hds.c`

- CRI pass 18b: SFHDS_SetHdr hard pins p r28 / len r29 + sfh r31 in the inlined SFHDS_IsSfdHeader (the three physical neighbours lift result/sfd to the next level: result r30, sfd r27) (COMPILER-DIFF: M1); sfhds_DoProcessHdr fixed in pass 13 by per-site if/else locals for the vid ternaries

## `lib/sfd_lib.c`

- SFD_Init: the two SFD_INIT_PRM word loads as `asm { lwz p1, 4(prm) }` / `asm { lwz tbl, 0(prm) }` on register locals (COMPILER-DIFF: M1, word 4 loaded first) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/sfd_mps.c`

- 26/26, pure C, M1 pin removed: sfmps_DecodeOneUnit 5 -> 0. The PES callback pair as `static sfmps_SetPesFns(sfd, mps)` with `obj = GetCond(PESOBJ); fn = GetCond(PESFN); MPS_SetPesFn(mps, fn, obj)` (obj first = the target's evaluation order): the helper's locals are inlined `@N` webs, so both call-result copies are compiler copies that coalesce into codeless ghosts (+2 never-removed neighbours on `ret`, exactly the pass-55 requirement); `n = (len < 0xB0) ? len : 0xB0`; `ret = err` plain

## `lib/sfd_mpv.c`

- CRI mpv pass 3: 38/38, pure C, no pins. ChkBufSiz 18 -> 0: `csize = (h16 / 2) * (cwidth = ...)` (the h16/2 sign chain created before the w16/2 chain: h16/2 r0 and ywidth r7 live across it, the cwidth temporaries colour r8 in place of w16). DecodePicAtr 197 -> 0: block-scoped `Sint64 dd` + `d = dd` at the block end (the frontend splits dd's subtraction off, the low word of the copy coalesces, the high word stays `mr r22, r21`, d keeps r20/r22 and the 16th callee-saved register goes away); the reform section as an inlined sfmpv_ReformTc (reform/newgop helper locals rank above ChkGopTc's ttu1) with the ttu1 block as an inlined sfmpv_ChkGopTc (flag's `li 0` CSE'd with the `d < 0` compare zero, emptied THEN arm = `bne; b`); `d < 0` (was `d > 0`); SFSEE_VRAW view of the header cache, `swk`/`wk` loads before the see.wk test, `len` local declared after `n`, `d = -1` before `pts = -1`, ttu3/ttu1b declaration vs statement order, picrate stored before bitrate, `vb = vbvsiz` before `br = bitrate`

## `lib/sfd_ply.c`

- SFD_Destroy returns SFTRN_CallTrSetup's result (r3 live across the hn-table clear -> loop r4/r5, sfd r31)

## `lib/sfd_pts.c`

- SFPTS_ReadPtsQue: eight hard-register asm pins (hn/-1 r7, rd r12, idx r4, st r3, cnt-i r3, &ent[idx] r3) (COMPILER-DIFF: M1) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/sfd_see.c`

- SFSEE_ExecServer: hard-register asm pins for the inlined wk/req (r29/r30) and the CalcByteRate wk reload (r29) (COMPILER-DIFF: M1) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/sfd_set.c`

- SFD_SetCond: id*4 as an asm-defined `register` local (takes the dead sfd register r28), hn declared first (COMPILER-DIFF: M1) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/sfd_tim.c`

- CRI pass 21: SFTIM_IsGetFrmTime 6 -> 0: a frame-taking copy of the inlined body (sftim_IsGetFrmTimeFrm) with `tunit` declared between `tscale` and `vrate` and read before `ftime` -- only a local of the same inlined body ranks between the body's locals (r10 between r9/r11); a wrapper local or the Tunit call's argument temporary ranks above them all

## `lib/sfd_tst.c`

- CRI pass 14: 10/11, SFTST_Calc 79w (the abs diamond anchored above the sftst_Conv call by the nested `diff =` assignment; residue = diff pair lo/hi ranking r23/r25 vs r25/r23: the backend propagates `mr diff, sub` so the pair are backend temps above adiff.hi); .rodata order kept by the named strings
- 11/11, pure C: SFTST_Calc 79 -> 0. `tol = mt->unit * tst->tolerance.cnt / tst->tolerance.unit` written out (a local assigned a plain copy of an inlined helper's result is replaced by the helper's return @temp, ranked above every own local; as an own local below `ave`, ave.hi is removed in the same Chaitin scan right after tol instead of one scan later, and MulDiv/diff/adiff/the sprintf group all shift to the target's registers); the `hist[i] -= step` loop as `static sftst_SubHist` (its `i` is a helper web coloured after the step temps: r6, not r3); sftst_SumHist declares `i` before `sum` (helper-local ids ascend in declaration order, the second inline's sum is coloured first: r4/r5)

## `lib/sfd_uo.c`

- SFUO_Create: channel clear in an inlined static helper called with `&sfd->uo_tbl` (zero-copy `i = 0`, the argument itself is the stepping pointer)

## `lib/sfh_main.c`

- 36/36: SFH_AnlyElemSmpHz 6 -> 0 (M4 stwbrx closed). The post-RA peephole's stwbrx rule follows a PER-BLOCK def table, so a block boundary between the swap chain and its `stw` keeps the vendor's `rlwinm/rlwimi x3/stw`: `w = SWAP32(..); ret = TRUE; z = 0; if (z != 0) ret = id; *val = w; return ret;` (tagged dead conditional: the frontend keeps own-local constants out of relational compares, pass 15 folds `li; cmpi; bt` into the fall-through and deletes the unreachable arm; `ret = TRUE` before the compare = `li r3,1` next to the load; the arm's `id` read keeps r4 live so the word takes r6). The six sfh_GetHdr* readers keep their pass-18b asm/peephole-off/pin form

## `lib/sfx_alp.c`

- SFXA_Create: constants, the sfxa_work address and the r0 temporaries (also the inlined search's) pinned with hard-register asm (COMPILER-DIFF: M1) — superseded 2026-09-17: the asm-emitted form is gone, the unit is C (docs/research/asm-removal.md).

## `lib/sfx_cnv.c`

- CRI pass 16b: the LUMI table loop is a static helper defined before SFX_MakeTable (its 1.164f literal and int->float constant are created before MakeTable's strings; the inlined helper's `i` shares the zero `li` and takes r4) and the conversion is `(Uint8)(1.164f * ...)` without the (Sint32) cast (the fctiwz slot/FPR order); pure C

## `lib/sfx_zmv.c`

- CRI pass 8 (pure-C revert): sfxzmv_MakeCnvZTbl 94w (M1: inlined helper src/dst r3/r4) / MakeOrgZ32TblByCCIR 77w (M1: unrolled 1.164f loop slot order)
- CRI pass 19b: 6/8, sfxzmv_MakeCnvZTbl 96 -> 12w (the helpers' src/dst declared dst-first and assigned src-first: src ranks above dst = r3/r4, tbl keeps its level-2 r31; left: the linear loops' copies in the guard block + one addi slot), MakeOrgZ32TblByCCIR 74w (the eight `i - k` computed up front in the target: a different DAG)
- 8/8, pure C: sfxzmv_MakeOrgZ32TblByCCIR 74 -> 0. One counter per loop + `d = tbl; *d++` in the last loop = nine own locals, so the 1.164 loop's counter is virtual r43: the post-schedule `addi rX,rX,K` sink (peephole-forward 0x5025e0) compares store data-register NUMBERS without the register class and stops at `stfd f43`, the block keeps its scheduled bit and is not rescheduled post-RA

## `lib/sndvd.c`

- SN libsn sndvd: DABR set through an "m" asm operand (stack slot + hard r3), DSIExcHandler as one top-level asm block, uncached DI address `(0x0C006000 + i*4) | 0xC0000000` inside the copy loop, empty asm keeping the default case's `bl ForceDvdDeIrq` from being cross-jumped (COMPILER-DIFF #6)

## `lib/svm.c`

- CRI SVM server manager: volatile lock/init counters (reloads), inlined svm_exec_svr helper (zero copies), dead setters/getters for the first-reference .bss order, `*p++ = 0` unrolled clear

## `lib/tealeaf.c`

- hand-written libsn/crt assembly (src/lib/*.s, see configure.py ASM_UNITS): local branch targets are resolved by the assembler, so objdiff shows ARG_MISMATCH on them; the linked bytes are identical

## `st1_2/r10c.cpp`

- SetEmHitAtari: dead hard-register 0.0 load at the top for the hoisted high (COMPILER-DIFF 3)

## `st1_2/r11b.cpp`

- Init: codeless asm issue-slot filler between the two flags_51BC RMWs (COMPILER-DIFF candidate)

## `st2_2/r213.cpp`

- Init: hard-register `&rot` memset argument + volatile asm behind the second memset (COMPILER-DIFF 3)

## `st2_3/r221.cpp`

- throwBonbe: single-set `evNo` slot filler for the eff2/pG-high r21/r22 tie (COMPILER-DIFF candidate #17)

## `st2_3/r223.cpp`

- reva_common_move: FP-before-mode definition under the mangled name (COMPILER-DIFF #8)

## `st2_3/r225.cpp`

- SceElevator: goto-entered noted up loop + volatile-asm cse flush in the RsfSet arm (COMPILER-DIFF candidate #12), dead pPL read, do-while(0) sched barrier

## `t_camera/t_camera_data.cpp`

- tcDataExport 27 -> 0 pure C (loop 5 `i++` in the header, the shared `dc` as the cut walker, a raw-word `*(u32*) &r->area` store, `static const char tag[]`); the loop-4 asm-emitted tcCdat base became unnecessary. 17/17.

## `t_esp/db_widget.cpp`

- db_widget DB_STRING ctor 2 -> 0 with four sched1-only `"=m"` anchors (flow1 keeps them, sched1 hoists the pool `lfs` above them, flow2 deletes them: vt qty life 12 -> 16, zero 5 refs > type) in place of the pass-12 launder whose `"m"(ca)` read raised the ca store in sched2. 113/113.

## `t_esp/t_esp.cpp`

- Load/SaveEmTypeUpdateCallback 2 -> 0 pure C. ModelTypeGroupSkip's loops step the global (`ModelTypeWrap(dir)`) and read it into a block-local u16 used only for the table index; the back-skip is a rotated `while` whose duplicated entry test cse2 folds completely (the copy's test-local pseudos are fresh, so loop 1's index/name classes are hit). 212/212.

## `t_id/t_id.cpp`

- toolIdOption: `col = (i == optCur) ? 4 : 0; sx = 0x2E;` before the menu-name eprintf (the optMenuName lo_sum misses loop pass 1's threshold by one and hoists in pass 2); pure C

## `t_movie/snd_test.cpp`

- disp_sequencer: `int y0 = 0x54` single-set REG_EQUIV constant ahead of `ch = 0` (one more preheader filler in LUID order), `y = y0 + 0x54` after the D diamond; pure C

## `t_movie/t_snd_vol.cpp`

- edit_reverb_param: p pin at the four helper call sites + efx_param_move as a macro (inlining drops RTX_UNCHANGING_P on pool loads)

