### Hazard: configure.py rewritten from a stale copy
- 10:22: the working tree's configure.py had lost the `"lib/sfd_mpv.c": deferred` CRI_CFLAG_OVERRIDES
  entry (uncommitted deletion, no agent owned it) and sfd_mpv.o failed in strip_unused. Never rewrite
  configure.py / modules.py / objects.py from memory: re-read immediately before editing and use a
  surgical edit. Restored from HEAD.

### Hazard: /tmp is a 32 GB tmpfs and `rm` is aliased to gio-trash in the interactive shell
- 2026-09-10 19:00: /tmp hit 26 GB from finished harness directories and a tools pass ran out of space.
  Use `/bin/rm -rf` (the plain `rm` alias refuses tmpfs). Orchestrator removed the finished passes'
  directories; agents should delete their /tmp harness when done or write large outputs under
  ~/.cache/<pass>/.
- 2026-09-11 01:00 cleanup: every finished pass's harness directory named in this file (/tmp/sched5, /tmp/cd6,
  /tmp/rank18 + /var/tmp/rank18, /tmp/gcse3, /tmp/equiv13, ~/.cache/ccfp7, ~/.cache/fold7, all /tmp/rooms_*,
  /tmp/ssw*, /tmp/dol*, /tmp/em*, ~/.cache/<pass> up to tools_p11/cri6/tesp2/rooms_c8, the
  re4-orig/sn-gcc-argorder/harness build outputs) was deleted. The paths in the research sections are
  historical; the findings are self-contained. To rebuild a whole-tree harness: copy tools/sn-gcc/src to
  ~/.cache/<name>/gcc, build cc1plus with tools/sn-gcc/Makefile, and drive it with a copy of a live pass's
  h.py/order.py (any current ~/.cache/<pass>/). Finished agents delete their own ~/.cache/<pass>/ at the end.

### Identity audit (2026-09-11): every unmatched unit byte-compared against its split object -- none identical; the true-residue list replaces the objdiff percentages

- Question answered: after db_light_v2 (a unit objdiff reported at 94-99% that was already byte-identical), are there other
  unflagged units that are in fact identical? No. All 66 units not flagged Matching at the end of the pass (53 DOL from
  objects.py, 13 REL from modules.py: Tools/db_mod, t_esp_area, t_lightarea, t_motseq, t_camera/t_camera_data, t_esp/db_mod,
  db_widget, t_esp, t_event/t_event, t_id/t_id, t_movie/snd_test, t_se_at, t_snd_vol) differ in code. game/emmine became
  identical (23/23) during the pass and was flipped by its own agent, not by the audit; no flag, source, or module symbol
  file was changed by the audit, `git diff config/G4BE08/symbols.txt` clean, `ninja -k 0` + `dtk shasum -c` 111 OK.
- The tool: `~/.cache/bytecmp.py <unit>` (`game/foo`, `lib/foo`, or `<mod>/<file>` for a REL unit; `--all` one line
  per unit, `--residue` the status-line form below, `<unit> FUNC` the word diff, `OBJ=` overrides our object). Per section
  it compares size, alignment and bytes with every relocation field masked, and every relocation TARGET by resolved
  location, never by name: a strong symbol defined in the object -> splits.txt section start + value + addend (module
  section offset for a REL); an UNDEF or WEAK symbol (linkonce vtables/functions: the link takes the FIRST copy, which is
  another unit's -- ctrl12's `_._7cCtrl12` references `_vt.5cUnit` at 0x8021D6A8, not its own copy at 0x8021D860) -> the
  linked ELF (build/G4BE08/main.elf; for a REL build/G4BE08/<mod>/<mod>.elf, then the module symbols.txt, then the DOL,
  then the linked modules by ascending id), then the dtk symbols.txt (`<name>_<address>` = dtk's spelling of a cross-unit
  reference to a scope:local symbol); a location inside the unit's own .text -> (function index, offset) when the
  function layouts are equal, (name, offset) otherwise; COMMON by addend only (make_rel allocates the block). Functions
  are paired by position when the (offset, size) layouts agree (so a placeholder-named row is a `name-only` note, not a
  diff), by canonical name otherwise; nameless .text regions (fold_linkonce's appended copies, the target's `fn_<mod>_<off>`
  blocks) are pseudo functions cut at the other side's boundaries. Unrelocated branches are keyed by their target, also
  when the original link resolved them into ANOTHER unit of the same section (negative displacement out of the object:
  the `bl countActiveWork` of r104's nameless block). A raw `lis rX, 0x80NN` the split lost the @ha pairing of compares
  equal when our resolved address folds to it; `GXWGFifo` (ABS) is folded the same way.
- Split-object artefacts that are NOT differences (all verified against the 962 Matching units: 961 IDENTICAL, the one
  exception game/sscrn's documented 12-byte `.text` pad of ppcdown's `.balign 32`): dtk gives every data section align 8
  (ours 4/1) -- reported, never a verdict; a section that is longer in the split by zero TAIL bytes (the next unit's
  alignment padding: adx_tsvr .rodata 0x98/0x96, quake .rodata 0x10/0xc) or by < 32 NOBITS bytes is "pad"; a split
  section that starts with k < 8 zero bytes and no relocation before k, after which the bytes equal ours (st4_0/cSceObj
  .rodata: the pin 0xAC4 is the previous object's end, our data starts at 0xAC8), is a "head pad" and our symbols are
  read at +k; a NOBITS/zero section only the split has (light10 .bss 0x4, snd_iss4 .data 0x4) is pad. dtk binds every
  split symbol GLOBAL, so scopes cannot be read off the split object (REL ADDR16 scopes: `make_rel.py --verify`).
- Reading the residue: objdiff's percentages were wrong in both directions -- t_esp/db_widget 93.6% is 112/113 with 11
  words in one constructor, lib/sfd_tim 99.97% is 6 words, game/dvd 99.9% is 7 words, while game/db_cam 98.7% is 368
  words in 4 functions and lib/sfd_hds 99.9% hides an 11-word SFHDS_SetHdr. `fn_<mod>_<off> N` rows in a REL unit whose
  layout differs are the nameless-block pairing (k-th block with k-th block): a 24-word row = one 0x60 block ours has at
  another position, not code. `(a/b)` after a count = target/ours function size.
- True residue at the end of the pass (`bytecmp.py --residue`; unit identical/total functions (differing words): function
  words (sizes) | section, order and data issues):
  - game/cam_ctrl 73/84 (691 w): HermiteExport__13CameraControlP9CameraCutPUc 192 (0x3f8/0x3e8), area_hit_pN__FP3VecP14CameraAreaInfo 118 (0x1f4/0x1c8), cameraHitCheck__FP3VecN30 116, areaHitCheck__13CameraControl 81 (0x470/0x444), area_hit_p3__FP3VecP14CameraAreaInfo 73 (0x168/0x158), checkAttachCamera__13CameraControl 38 (0x1d8/0x1cc), r0_RailBehind__13CameraControl 36, switchCamera__13CameraControlP13CameraAreaRec 20, roomInit__13CameraControl 15, create__t8cManager1Z6cLighti 1, create__t8cManager1Z6cLight 1 | ORDER; .text size 0x62e8/0x639c; .rodata bytes: first diff @0x138, 3 words
  - game/card 64/67 (186 w): errorDisp__5cCard 99 (0x854/0x83c), saveMain__5cCard 74 (0x734/0x740), makeCardStatus__5cCardP8CardSlot 13 | .text size 0x7850/0x7844
  - game/option 17/20 (196 w): controller_menu 82, brightness_menu 69 (0x6c4/0x6c8), retry_load_menu 45 | .text size 0x2c68/0x2c6c
  - lib/adx_tsvr 5/6 (2 w): adxt_nlp_trap_entry 2 | pad .rodata 0x98/0x96
  - lib/adx_bsc 31/36 (1324 w): ADXB_DecodeHeaderAdx 1009 (0xb70/0xb50), ADXB_ExecOneAdx 212 (0x370/0x36c), ADXB_EvokeDecode 94 (0x18c/0x174), ADXB_ExecHndl 7, ADXB_Stop 2 | .text size 0x1888/0x184c; .rodata size 0x888/0x87b; pad .data 0x8/0x0
  - lib/adx_dcd5 1/4 (337 w): ADX_DecodeSte4AsMono 180 (0x2f0/0x2ec), ADX_DecodeSte4AsSte 118, ADX_DecodeMono4 39 | .text size 0x708/0x704
  - lib/cri_cvfs 11/13 (211 w): cvFsOpen 148 (0x664/0x660), cvFsGetFileSize 63 | .text size 0x13f8/0x13f4; pad .bss 0x488/0x484
  - lib/adx_baif 5/6 (170 w): AIFF_GetInfo 170 (0x270/0x268) | .text size 0xb78/0xb70; pad .rodata 0x10/0xd
  - game/act_btn 8/10 (110 w): checkButton__13cActionButtonP10ActBtnWork 93 (0x26c/0x254), disp__13cActionButtonP10ActBtnWork 17 | .text size 0x8d4/0x8bc
  - game/at_mod 16/18 (95 w): ObaLineHitChk 73 (0x3d0/0x3e0), ComnHitCheck 22 (0x188/0x184) | .text size 0x1c50/0x1c5c
  - game/cam_extra 32/43 (861 w): move__11IdBinocularPv 338 (0x78c/0x790), move__16CameraPushObject 131 (0x44c/0x438), move__14FocusAnimationi 97 (0x208/0x1d8), move__15CameraBinocular 97 (0x3d0/0x3cc), move__11CameraScope 91 (0x4d8/0x4dc), move__22CameraAttachedToMotion 62 (0x230/0x22c), __15CameraBinocularP3VecT1PvT3 31, init__14FocusAnimationi 8, __16CameraLookDownEmPvP3Vec 4, _._7cCamera 1, _._13IDApplication 1 | .text size 0x2ad4/0x2a90; .rodata size 0x310/0x308 (zero tail padding) bytes: first diff @0xe4, 44 words reloc targets differ @0x1a4:  vs (1, (.text, _._16CameraLookDownEm, 0)) (+35 more); pad .rodata 0x310/0x308
  - game/db_cam 9/13 (368 w): menuFlag__11debugCameraP3JOY 144, move__11debugCameraP6CameraP3JOYi 141 (0x968/0x970), menu__11debugCameraP6CameraP3JOY 72 (0x334/0x330), adjust_qFPS__FP3JOYiiiPi 11 (0x11fc/0x11f8)
  - game/debug 6/11 (403 w): processBarDisp 165 (0x8f8/0x8f0), ConfigSet 165 (0xe24/0xe30), debugPadInfoDisp 34 (0xe8/0xf4), PrimitiveBuffDisp 31, ProcessTickGet 8 | .text size 0x1ea4/0x1eb4
  - game/dvd 54/55 (7 w): DiscChange__4cDvdi 7
  - game/em_cloth 21/23 (56 w): Em18ClothSet 47, Em34ClothSet1 9
  - game/em_set 10/12 (147 w): EmSetEvent__FP10EmListData 74, EmSetFromList2 73 | pad .rodata 0x1e8/0x1e4
  - game/em_sub 41/51 (807 w): GetDropBullet 492 (0xb7c/0xb24), EmYarareContactCk 103, RandomItemCk 53, EmRackCk 49, GetWepTargetList2 35, GetWepTargetListBomb 32, GetWepTargetList 20 (0x254/0x258), emLineCapsuleCrossCk 14, EmCatchMotionMove 8 (0x150/0x14c), EmYarareDisp 1 | .text size 0x7b5c/0x7b04
  - game/emrock 53/58 (90 w): emRockDropCamMove 36, plemRockDropDieCamMove 20 (0x15c/0x158), plemRockEscapeCamMove2 15 (0x1e0/0x1dc), emRockPushCamMove 10, emRockRollStartCk 9 (0xd0/0xcc) | .text size 0x524c/0x5240; .rodata reloc targets differ @0x4fc: (1, (.text, _._7cEmRock, 0)) vs (1, (.text, _._7cEmRock, 0xc)) (+0 more)
  - game/Espgen42 11/16 (622 w): Espgen42_Move00 444 (0x86c/0x894), SetWaterWork 73, AddWaterPowerSub 72 (0x60c/0x600), AddWaterPower 26, GetWaterHeightSub 7 | .text size 0x2f9c/0x2fb8
  - game/Espgen43 9/11 (49 w): SetSandWork 44, AddSandPower 5
  - game/espgen45 17/20 (1297 w): Espgen45_TransSub 752 (0x1b40/0x1bd0), Espgen45_Move00 454 (0xa54/0xa7c), SetWaterWork45 91 | ORDER; .text size 0x3720/0x3878
  - game/main_mem 30/33 (173 w): MemCheckUsedHeap 143 (0x1290/0x128c), MemReplaceHeap__Fii 18, MemCheckHeapEnd__Fi 12 (0x9c/0x98) | .text size 0x20b0/0x20a8
  - game/motion 25/31 (181 w): MotionHokan 46 (0x5d4/0x5d0), MotionMove 34 (0x85c/0x858), MotionSetCore__FP6cModelPvT1iiii 33 (0xa10/0xa0c), MotionGetPosition 30, HermiteInterpolation 26 (0x2cc/0x2c8), MotionSequenceCtrl 12 | .text size 0x3550/0x3540; .rodata relocs: 2/0 entries reloc targets differ @0x218: (1, (addr, 0x8023ed40)) vs  (+1 more)
  - game/pendulum 7/12 (1066 w): PenClothMove 459, PenClothMove3 276, PenClothMove2 247, penClothAtCkParallel 68, penClothAtMake 16
  - game/pl_wep 24/29 (499 w): PlWepHitCheck2__FP6cModelP3VecT1iUlf 242 (0x61c/0x5c4), PlWepLockCtrl__FP6cModel 189 (0x65c/0x650), PlWepAutoTrack 36 (0x254/0x258), searchLockEm 21, PlSetLockPitch 11 | .text size 0x21f8/0x2198; .rodata size 0x350/0x290 bytes: first diff @0x108, 17 words
  - game/puzzle 38/49 (979 w): movePiece__9pzlPlayer 535 (0xca8/0xc34), PutInCase 222, init__9pzlPlayeri 83 (0x3ac/0x394), cmbPiece__9pzlPlayerP8pzlBoard 36, shape__8pzlPieceii 30, rmPiece__8pzlBoardP8pzlPiece 24, putPiece__8pzlBoardP8pzlPiece 22, selPiece__9pzlPlayerP8pzlBoard 17, init__8pzlBoardiii 4, removeExtraPiece__9pzlPlayer 4, appendExtraPiece__9pzlPlayerP8ItemWork 2 | .text size 0x3438/0x33ac; .rodata size 0x280/0x260 bytes: first diff @0x78, 122 words
  - game/route_ck 10/16 (108 w): RouteCkToPos 36, RouteCkToEm 26, RouteCkEscEm 18, Draw_rtp 15 (0x548/0x544), RouteCkPosToPosDis 8 (0x174/0x16c), RouteCkPosToPos 5 | .text size 0x1b98/0x1b8c
  - game/sce_at 111/114 (120 w): sceAtGetItem 98, sceAtGetItem_NoModel 12, SceAtCheckSystemItemSet 10 | ORDER; .text size 0x8e54/0x8e8c
  - game/sce_com 28/33 (442 w): SceElevator 232 (0x6dc/0x6d4), OpenBoxMain 182, SceSetItemEvent 23, SceEventStart 3, SceEventEnd 2 | .text size 0x2de0/0x2dd8
  - game/shadow 32/36 (29 w): MakeSoftShadow 24 (0x204/0x1f4), make_comn_fit_light 2, make_comn_parallel_light 2, ShdInit 1 | .text size 0x4a48/0x4a38
  - game/t_bugcheck 4/7 (34 w): menuLife__13cToolBugcheck 18, menu__13cToolBugcheck 14, menuPosMove__13cToolBugcheck 2
  - game/esp08 4/6 (745 w): Esp08_TransShimmer 440 (0x1b8c/0x1ba4), Esp08_Trans 305 (0x1444/0x1454) | .text size 0x3378/0x33a0; .rodata reloc targets differ @0x264: (1, (.text, _._6cEsp08, 0)) vs (1, (.text, SetFreeWork__6cEsp08P10EspGenWorkPUl, 0x12c)) (+0 more); pad .sdata 0x10/0xc
  - game/esp18 4/5 (322 w): Esp18_Trans 322 (0x1180/0x1168) | .text size 0x1268/0x1250; .rodata reloc targets differ @0x25c: (1, (.text, _._6cEsp18, 0)) vs (1, (.text, _._6cEsp18, 0x18)) (+0 more)
  - lib/mwsfdcre 4/10 (1088 w): mwPlyCreateSofdec 551 (0x1078/0x105c), mwsfcre_CreateSfd 389 (0xe70/0xe40), mwsfcre_MallocCompoWork 69, mwsfcre_MallocRfb 32 (0x2a4/0x2a0), mwPlyCalcWorkSfd 30, mwPlyCalcWorkCprmSfd 17 (0x84/0x80) | .text size 0x2c28/0x2bd4; pad .rodata 0x528/0x521, .data 0x158/0x154, .bss 0x70/0x6c
  - lib/sfx_cnv 2/4 (141 w): SFX_MakeTable 139 (0x478/0x47c), sfxcnv_IsCnvUpHalf 2 | .text size 0x5d4/0x5d8; .rodata size 0xd0/0xc8 bytes: first diff @0x5c, 27 words
  - lib/sfx_zmv 6/8 (170 w): sfxzmv_MakeCnvZTbl 96, sfxzmv_MakeOrgZ32TblByCCIR 74 | pad .rodata 0x140/0x13f, .bss 0x270/0x26c
  - lib/sfd_adxt 22/28 (293 w): sfadxt_ExecServerSub 112 (0x428/0x42c), sfadxt_AdjustSync 69 (0x280/0x27c), SFADXT_SetSpeed 42, sfadxt_ExcludeHdr 38, SFADXT_Create 20, SFADXT_Pause 12 | pad .rodata 0x68/0x67, .bss 0x28/0x24
  - lib/sfd_buf 21/26 (541 w): SFBUF_InitHn 447 (0x6d0/0x6a8), SFBUF_SetSupplySj 36 (0x1b8/0x1ac), SFBUF_RingAddRead 22, SFBUF_DestroySj 20 (0xcc/0xb8), SFBUF_RingAddWrite 16 | .text size 0x1590/0x1548
  - lib/sfd_cre 3/6 (86 w): sfcre_AnalyAudio 43, sfcre_AnalyMps 28, sfcre_AnalyMpv 15 | pad .rodata 0x158/0x154
  - lib/sfd_hds 10/11 (11 w): SFHDS_SetHdr 11 | pad .bss 0x208/0x204
  - lib/sfd_mps 21/26 (622 w): sfmps_DecodeOneUnit 316 (0x674/0x67c), sfmps_CopyPketData 157, sfmps_CopyPrvate 60, sfmps_ExecServerSub 59, sfmps_ProcPrep 30 | .text size 0x1ce0/0x1ce8; .rodata reloc targets differ @0x0: (1, (.text, SFMPS_Init, 0)) vs (1, (.text, sfmps_CopyAudio, 0)) (+17 more); pad .bss 0x818/0x814
  - lib/sfd_mpv 14/38 (3136 w): sfmpv_ChkBufSiz 463 (0x530/0x618), sfmpv_DecodePicAtr 454 (0xaf8/0xb00), sfmpv_DecodeOneUnit 377 (0x5b0/0x80c), sfmpv_CalcRepeatField 215 (0x4d0/0x460), sfmpv_InitInf 207 (0x388/0x390), sfmpv_DecodeFrm 185 (0x510/0x500), sfmpv_Concat 170 (0x348/0x340), sfmpv_GetActiveSize 159 (0x768/0x7a4), sfmpv_IsSkip 151 (0x430/0x424), sfmpv_DoReformTc 120 (0x250/0x258), SFMPV_Create 104 (0x270/0x280), sfmpv_Pts2Tc 102 (0x1dc/0x1e4), sfmpv_GoDdelim 84 (0x290/0x298), sfmpv_NeedSafeDlmRefresh 84 (0x3a4/0x3dc), SFD_SetPicUsrBuf 64 (0x1a4/0x1a8), sfmpv_ExecServerSub 57 (0x3d4/0x3dc), SFMPV_Destroy 40, SFMPV_Seek 38, sfmpv_SetFrmPara 27, SFMPV_SaveCond 19 (0x9c/0x98), sfmpv_SetFrmInf 8 (0x264/0x268), SFD_CalcYccPlane 5, SFMPV_Stop 2 (0x10/0x18), SFMPV_GetRead 1 | .text size 0x52ec/0x565c; pad .rodata 0xa0/0x9c, .bss 0x120a0/0x1209c
  - lib/sfd_tim 38/39 (6 w): SFTIM_IsGetFrmTime 6
  - lib/sfd_tst 10/11 (83 w): SFTST_Calc 83 | pad .rodata 0x1a0/0x19f, .bss 0x18/0x14
  - lib/sfh_main 29/36 (36 w): SFH_AnlyElemSmpHz 6 (0x15c/0x14c), SFH_AnlyMaxFrmNum 5, SFH_AnlyMaxPlyLenVid 5, SFH_AnlyMaxPlyLenAud 5, SFH_AnlyByteRate 5, SFH_AnlyPackSiz 5, SFH_AnlyHdrSiz 5 | .text size 0x29b8/0x29a8; pad .rodata 0x98/0x95, .bss 0x18/0x14
  - lib/cftyp422_ppc 2/8 (230 w): CFT_Ycc420plnToY84C44 179, CFT_Ycc420plnToArgb8888Init 24, CFT_MakeArgb8888AlpLumiTbl 9, CFT_MakeArgb8888Alp3211Tbl 8, CFT_MakeArgb8888Alp3110Tbl 8, cnvStaticYcc420plnToArgb8888 2 | .rodata size 0x88/0x8c; pad .data 0x8/0x4, .bss 0x1428/0x1420
  - lib/cftfx 3/6 (233 w): cnvDynamicYcc420plnToA256UserTable 135 (0x22c/0x214), cnvStaticYcc420plnToA256V 60, CFT_Argb420ToArgb8 38 (0x2f8/0x2f0) | .text size 0xb8c/0xb6c; pad .rodata 0x20/0x1c, .bss 0x358/0x354
  - lib/mps_lib 6/7 (2 w): MPS_Create 2 | pad .bss 0x10/0xc
  - lib/mpv_umc 11/16 (144 w): mpvumc_OneReadMb 91, MPVUMC_Intra 35, MPVUMC_Backward 8 (0xb4/0xa8), MPVUMC_Forward 8 (0xb4/0xa8), MPVUMC_BiDirect 2 | .text size 0x156c/0x1554; pad .rodata 0x8/0x4
  - lib/mpv_mcy 1/5 (85 w): MPVMC16_OneRefH2_TuneC 31 (0x470/0x46c), MPVMC16_OneRef4p_TuneC 26, MPVMC16_OneRefV2_TuneC 18, MPVMC16_OneRef1p_TuneC 10 | .text size 0x11e8/0x11e4
  - lib/mpv_cmc 2/4 (24 w): MPVCMC_InitObj 16 (0x8c/0x88), MPVCMC_InitMcOiRt 8 (0x40/0x3c) | .text size 0x134/0x12c
  - lib/dct_ac 2/3 (45 w): DCT_AcInit 45 (0x100/0xf4) | .text size 0x3dc/0x3d0; .rodata bytes: first diff @0x9, 8 words; pad .bss 0x408/0x404
  - lib/adx_sje 11/17 (249 w): adxsje_set_rsig 85, adxsje_encode_data 84, adxsje_calc_rsig 41 (0x4c8/0x4cc), ADXSJE_ExecHndl 35, adxsje_output_header 2, adxsje_write_end_code 2 | .text size 0x21c8/0x21cc; pad .rodata 0x70/0x6f
  - Tools/db_mod 45/63 (2246 w): dbmod_locate__Fv 475 (0x11dc/0x11fc), dbmodDispModelName 227 (0x648/0x604), dbmodGetFilenames 222 (0x78c/0x778), dbmod_motion__Fv 212 (0x848/0x81c), dbModMotionMove 194 (0xa54/0xa48), dbmod_blend__Fv 185 (0x5d8/0x5dc), dbmod_p_info__Fv 159 (0x638/0x62c), drawOrientation 115 (0x4e0/0x4c8), dbmodInfoDisp 107 (0x334/0x2e0), dbmod_option__Fv 88 (0x590/0x594), dbmod_light__Fv 81 (0x4c4/0x4c8), dbmod_trans__Fv 71 (0x2ac/0x2a0), dbmod_scale__Fv 64 (0x2d4/0x2d8), fn_Tools_1B774 24, position_usage 10 (0x2f0/0x2e8), IKreport__5DB_EM 10 (0xe8/0xe4), loadModel__5DB_EM 1, create__t8cManager1Z3cEmi 1 | .text size 0x9978/0x9888
  - Tools/t_esp_area 32/38 (470 w): ToolEspArea__Fv 438 (0x1c50/0x1c44), fn_Tools_2683C 24, execCopyWindow__t14cDbgEditWindow1Z8ESP_AREA 3, LocalUpdate__t14cDbgEditWindow1Z8ESP_AREA 2, fn_Tools_28718 2, cutBuffer__t14cDbgEditWindow1Z8ESP_AREA 1 | .text size 0x4524/0x4518; .rodata reloc targets differ @0x4a4: (1, (.text, _._t14cDbgEditWindow1Z8ESP_AREA, 0)) vs (1, (.text, _._t14cDbgEditWindow1Z8ESP_AREA, 0xc)) (+14 more)
  - Tools/t_lightarea 31/38 (484 w): ToolLightAreaMain__21t_lightarea_namespacev 393 (0x1fc8/0x1fbc), fn_Tools_30410 59, fn_Tools_2F2D8 24, execCopyWindow__t14cDbgEditWindow1Z10LIGHT_AREA 3, LocalUpdate__t14cDbgEditWindow1Z10LIGHT_AREA 2, fn_Tools_31174 2, cutBuffer__t14cDbgEditWindow1Z10LIGHT_AREA 1 | .text size 0x4c18/0x4c0c; .rodata reloc targets differ @0x50c: (1, (.text, _._t14cDbgEditWindow1Z10LIGHT_AREA, 0)) vs (1, (.text, _._t14cDbgEditWindow1Z10LIGHT_AREA, 0xc)) (+10 more)
  - Tools/t_motseq 16/21 (353 w): msqDisp__Fv 147 (0xf58/0xf40), msq_R0_Sequence__Fv 97 (0x92c/0x928), msq_R0_SeqResize__Fv 78 (0x1d8/0x1d4), fn_Tools_34F9C 24, msq_R0_QuitCk__Fv 7 (0xe8/0xe0) | .text size 0x3260/0x3238
  - t_camera/t_camera_data 14/17 (193 w): tcDataExport__FPUc 142 (0x538/0x534), tcSetBesideOffset__FPA2_7QfpsOfsT0 27, fn_t_camera_1B8C4 24 | .text size 0x1600/0x15fc
  - t_esp/db_mod 57/75 (2246 w): dbmod_locate__Fv 475 (0x11dc/0x11fc), dbmodDispModelName 227 (0x648/0x604), dbmodGetFilenames 222 (0x78c/0x778), dbmod_motion__Fv 212 (0x848/0x81c), dbModMotionMove 194 (0xa54/0xa48), dbmod_blend__Fv 185 (0x5d8/0x5dc), dbmod_p_info__Fv 159 (0x638/0x62c), drawOrientation 115 (0x4e0/0x4c8), dbmodInfoDisp 107 (0x334/0x2e0), dbmod_option__Fv 88 (0x590/0x594), dbmod_light__Fv 81 (0x4c4/0x4c8), dbmod_trans__Fv 71 (0x2ac/0x2a0), dbmod_scale__Fv 64 (0x2d4/0x2d8), fn_t_esp_1BF24 24, position_usage 10 (0x2f0/0x2e8), IKreport__5DB_EM 10 (0xe8/0xe4), loadModel__5DB_EM 1, create__t8cManager1Z3cEmi 1 | .text size 0xa0ec/0x9ffc
  - t_esp/db_widget 112/113 (11 w): __9DB_STRINGUlPCc 11
  - t_esp/t_esp 195/212 (13061 w): InitTool__15t_esp_namespacev 12050 (0xa20c/0x9ebc), __Q215t_esp_namespace9ID_WINDOWP13DB_PRIM_ARRAY 254, AddSeq__15t_esp_namespaceP8TOOL_SEQN21 197 (0x17d4/0x17c4), SaveEmTypeUpdateCallback__15t_esp_namespaceP12DB_PRIMITIVE 96, LoadEmTypeUpdateCallback__15t_esp_namespaceP12DB_PRIMITIVE 87, ToolEspMain__15t_esp_namespacev 72 (0x8b8/0x8b0), PosActiveChange_callback__15t_esp_namespaceP9DB_WINDOWP12DB_PRIMITIVEP10DB_KEYBORD 62 (0x4ec/0x4dc), MakeSaveSeqData__15t_esp_namespaceP10EspSeqDataP8TOOL_SEQUlUl 58, EditActiveChange_callback__15t_esp_namespaceP9DB_WINDOWP12DB_PRIMITIVEP10DB_KEYBORD 50 (0x4bc/0x4c0), PartPasteSeqData__15t_esp_namespaceP8TOOL_SEQUlT1 46 (0x488/0x490), EspToolMain__15t_esp_namespacev 40 (0x3f4/0x408), fn_t_esp_3DE4C 24, SaveEmFileNoUpdateCallback__15t_esp_namespaceP12DB_PRIMITIVE 5 (0xcc/0xc8), SaveRoomFileNoUpdateCallback__15t_esp_namespaceP12DB_PRIMITIVE 5 (0xcc/0xc8), SaveSstFileNoUpdateCallback__15t_esp_namespaceP12DB_PRIMITIVE 5 (0xcc/0xc8), SaveEventFileNoUpdateCallback__15t_esp_namespaceP12DB_PRIMITIVE 5 (0xcc/0xc8), SaveEventSNoUpdateCallback__15t_esp_namespaceP12DB_PRIMITIVE 5 (0xcc/0xc8) | .text size 0x1562c/0x152c0
  - t_event/t_event 59/69 (1095 w): SubToolMessMove__7ToolEvtP7ToolEvtP5Event 346 (0x1010/0x1014), SubToolMessInit__7ToolEvtP7ToolEvti 212 (0x10a0/0x1074), MainPreview__7ToolEvtP7ToolEvt 195 (0x8c0/0x894), CallbackSave__FPv 174 (0x454/0x458), CallbackLoad__FPv 136 (0x56c/0x560), fn_t_event_1B528 24, execCopyWindow__t14cDbgEditWindow1ZQ216EventMessageData8MessElem 3, LocalUpdate__t14cDbgEditWindow1ZQ216EventMessageData8MessElem 2, fn_t_event_1D370 2, cutBuffer__t14cDbgEditWindow1ZQ216EventMessageData8MessElem 1 | .text size 0x7320/0x72c4; .rodata reloc targets differ @0x8c4: (1, (.text, _._t18cDbgButtonTemplate1ZQ216EventMessageData8MessElem, 0)) vs (1, (.text, _._t14cDbgEditWindow1ZQ216EventMessageData8MessElem, 0xc)) (+14 more)
  - t_id/t_id 52/69 (1206 w): idEditPos 364 (0x10d4/0x1078), idEditColor 201 (0xa04/0x9e8), idEditTrans 176 (0x540/0x544), toolIdOption__FP6IdTool 146 (0x594/0x574), idEditUnit 79, toolIdEdit__FP6IdTool 58, toolIdInit__FP6IdTool 53 (0x2a4/0x298), toolIdPaste 45 (0x1dc/0x1e0), toolIdDrawSafeZone 31 (0x1f4/0x1ec), ToolInterfaceDesign__Fv 25 (0x138/0x130), idEditId 12, toolIdEditDisp 4, __static_initialization_and_destruction_0 4, _._5cUnit 3, _._6cCoord 2, _._7ID_DATA 2, create__t8cManager1Z6cLighti 1 | .text size 0x9aec/0x9a40; .rodata reloc targets differ @0xaa4: (1, (.text, _._7ID_DATA, 0)) vs (1, (.text, global_destructors, 0x14)) (+10 more)
  - t_movie/snd_test 53/77 (559 w): disp_sit_normal__FP11SND_ISS_BLKP7SND_SITii 123 (0x684/0x674), disp_sequencer__Fv 64 (0x5a0/0x58c), disp_se_wt_data__FP11SndTestWorkP11SND_ISS_BLKP7SND_SIT 46, snd_test_disp_rit__Fv 37 (0x638/0x634), dir_entry_read__FP11SndTestWorki 37 (0x124/0x114), test_blk_enable_ck__FP11SndTestWorki 31, Snd_test_disp_voice__FP11SndTestWork 31 (0xf0/0xf4), Snd_test_disp_menu__FP11SndTestWork 26, Snd_test_disp_efx__Fv 20, disp_adsr_para__FP11SndTestWork 18, test_disp_efx_delay__FP11SndTestWorkP12SND_EFX_WORKii 18, Snd_test_disp_vol__Fv 16, Snd_test_disp_req_para__FP11SndTestWork 14, test_disp_efx_rev_hi__FP11SndTestWorkP12SND_EFX_WORKii 12, directory_disp__FP11SndTestWork 12, test_disp_efx_rev_std__FP11SndTestWorkP12SND_EFX_WORKii 10, test_disp_efx_rev_dpl2__FP11SndTestWorkP12SND_EFX_WORKii 10, blk_file_disp__FP11SndTestWork 10, aram_dump_disp__FP11SndTestWork 9, test_disp_efx_chorus__FP11SndTestWorkP12SND_EFX_WORKii 6, Snd_test_disp_aux__FP11SndTestWork 3, test_play_or_stop__FP11SndTestWork 2, load_select__FP11SndTestWork 2, cursor_disp__FP11SndTestWork 2 | .text size 0x5120/0x50ec; .rodata size 0x1004/0x1000 bytes: first diff @0x6d4, 583 words relocs: 1/0 entries reloc targets differ @0x6d4: (1, (addr, 0x802a2c64)) vs  (+0 more); .data reloc targets differ @0x5f0: (1, (t_movie, .rodata, 0x2060)) vs (1, (t_movie, .rodata, 0x205c)) (+22 more)
  - t_movie/t_se_at 18/20 (82 w): seAtAreaEdit_DataInput__Fv 58 (0xc6c/0xc68), fn_t_movie_19D18 24 | .text size 0x2e14/0x2e10
  - t_movie/t_snd_vol 21/27 (1635 w): edit_reverb_param__Fv 812 (0x15e8/0x1018), file_save__Fv 330 (0x804/0x8a4), data_edit__Fv 152 (0x884/0x868), file_load__Fv 122 (0x784/0x780), combine_tbl_edit__Fv 115 (0x3c0/0x344), combine_tbl_disp__FP7CombSel 104 (0x464/0x44c) | .text size 0x616c/0x5b88
