// genere par tools/re4/gen_addr_table.py : debug, USA, Europe
    {0x8001A614u, 0x80018844u, 0x80018C08u},  // ItemClear
    {0x8001AA94u, 0x80018CC4u, 0x80019044u},  // SetChar
    {0x80043E5Cu, 0x8003E170u, 0x8003EA00u},  // drawLaserSight
    {0x800443B4u, 0x8003E638u, 0x8003EEC8u},  // getMarkerPos
    {0x80057280u, 0x80050730u, 0x8005144Cu},  // PlayerInit (eur : Ghidra H_EU_Retail FUN_8005144c)
    {0x800579B4u, 0x80050DE8u, 0x80051A24u},  // cPlayer::move
    {0x80059D10u, 0x80053098u, 0x80053D4Cu},  // pl_R1_Aux
    {0x8005C698u, 0x80055944u, 0x8005661Cu},  // SndCall
    {0x80073CF8u, 0x80069760u, 0x8006A49Cu},  // DCFlushRange boucle
    {0x80073D00u, 0x80069768u, 0x8006A4A4u},  // DCFlushRange boucle fin
    {0x80078B38u, 0x8006E594u, 0x8006F450u},  // SelectThread inactif
    {0x80078B58u, 0x8006E5B4u, 0x8006F470u},  // SelectThread inactif fin
    {0x800823CCu, 0x80077D74u, 0x80078C88u},  // CompareDiskID
    {0x800BBBACu, 0x800AA354u, 0x800AB0F8u},  // gx site
    {0x800E7884u, 0x800C388Cu, 0x800C4A70u},  // DvdReadN (usa : appel ; eur : appel)
    {0x800E9E84u, 0x800C5B9Cu, 0x800C6D80u},  // cDvd::ReadCheck
    {0x800EB7C8u, 0x800C70D4u, 0x800C82F4u},  // EspDataLoad (usa : appel ; eur : appel)
    {0x800ED258u, 0x800C81B8u, 0x800C954Cu},  // cEmMgr::construct
    {0x800F3C78u, 0x800CE368u, 0x800CF708u},  // GetWepTargetPos
    {0x8012BDA0u, 0x80103C38u, 0x80105A90u},  // EstSet
    {0x80137520u, 0x8010CEF8u, 0x8010ED64u},  // Event::ExeBeginEvt
    {0x80139BD8u, 0x8010F010u, 0x80110FE8u},  // EventMgr::SetBin (usa : motif -1 ; eur : motif -1)
    {0x80142F7Cu, 0x801166F8u, 0x801187C8u},  // gameInit
    {0x8014339Cu, 0x80116B48u, 0x80118CB4u},  // gameRoomInit
    {0x80149174u, 0x8011A25Cu, 0x8011C66Cu},  // IDSystem::unitPtr (usa : appel ; eur : appel)
    {0x801532D8u, 0x801225DCu, 0x80124AACu},  // vsync 1 (usa : motif ; eur : motif)
    {0x801532ECu, 0x801225F0u, 0x80124AC0u},  // vsync 1 fin (usa : motif ; eur : motif)
    {0x801532F4u, 0x801225F8u, 0x80124AC8u},  // vsync 2 (usa : motif ; eur : motif)
    {0x80153304u, 0x80122608u, 0x80124AD8u},  // vsync 2 fin (usa : motif -2 ; eur : motif -2)
    {0x8015394Cu, 0x80122A84u, 0x80125018u},  // GetSystemVcnt
    {0x80153960u, 0x80122A98u, 0x8012502Cu},  // GetSystemVcnt fin
    {0x801542E0u, 0x801233C8u, 0x801259B8u},  // MemCreateHeap (usa : appel ; eur : appel)
    {0x80154A70u, 0x801239E0u, 0x80125FD0u},  // MemAlloc (usa : motif -7 ; eur : motif -7)
    {0x8015A38Cu, 0x80127510u, 0x80129B98u},  // cModel::addModel
    {0x8015B838u, 0x80128710u, 0x8012AD98u},  // cModInfoMgr::create
    {0x8015C878u, 0x80128AD0u, 0x8012B158u},  // cModel::getPartsPtr (usa : appel ; eur : appel)
    {0x8015D088u, 0x80128DE8u, 0x8012B480u},  // MotionSetCore
    {0x8015DA98u, 0x801297C4u, 0x8012BE5Cu},  // MotionMove
    {0x80173A98u, 0x8013DB94u, 0x80140260u},  // cPlayer::setDamage
    {0x80175A7Cu, 0x8013FA6Cu, 0x80142290u},  // cMot3::move
    {0x80178494u, 0x801423B4u, 0x80144CD4u},  // joyKamae
    {0x801785DCu, 0x801424BCu, 0x80144DDCu},  // joyLKamae
    {0x80179024u, 0x80142FE8u, 0x80145A0Cu},  // PlWepHitCheck2
    {0x8017978Cu, 0x80143794u, 0x801461B8u},  // getPitch
    {0x8017A154u, 0x8014415Cu, 0x80146B80u},  // PlWepLockCtrl
    {0x8017A7C0u, 0x801447B8u, 0x80147284u},  // PlWepLockRand
    {0x801A07F0u, 0x80161738u, 0x80164554u},  // titre (tache)
    {0x8024DC30u, 0x801EFD90u, 0x801F4478u},  // ClampRegion
    {0x8025F394u, 0x802005B0u, 0x80204E50u},  // g_transOfs
    {0x80266AC0u, 0x80207380u, 0x8020BD24u},  // Key_type_tbl (usa : voisin 80266AA0 ; eur : voisin 80266AA0)
    {0x8026AFE0u, 0x8020B134u, 0x8020FAD8u},  // EmFileTbl (usa : voisin 8026AFD0 ; eur : voisin 8026AFD0)
    {0x8026B5E0u, 0x00000000u, 0x00000000u},  // EmFileTbl_Klauser (usa : inconnue ; eur : inconnue)
    {0x80271A88u, 0x802138E0u, 0x802182C0u},  // ItemMgr
    {0x80271F34u, 0x80213D74u, 0x80218758u},  // cMes
    {0x80271F38u, 0x80213D78u, 0x8021875Cu},  // cMes.mes (usa : voisin 80271F34 ; eur : voisin 80271F34)
    {0x80276844u, 0x80218650u, 0x8021D888u},  // mot3
    {0x8027692Cu, 0x80218738u, 0x8021D970u},  // m3r
    {0x80277F10u, 0x802192E4u, 0x8021E51Cu},  // SubScreenWk
    {0x802A10C0u, 0x802420A0u, 0x802472E0u},  // Snd_ctrl_work
    {0x802D7B04u, 0x80278AE4u, 0x8027DD24u},  // ActBtn
    {0x802D90F0u, 0x80279320u, 0x8027E564u},  // Cckpt.action.no
    {0x802DA1CCu, 0x80279DB8u, 0x8027EFF8u},  // Dvd
    {0x802DE700u, 0x8027DF20u, 0x80283160u},  // EmMgr
    {0x802DE704u, 0x8027DF24u, 0x80283164u},  // EmMgr+4 (usa : voisin 802DE700 ; eur : voisin 802DE700)
    {0x802DE708u, 0x8027DF28u, 0x80283168u},  // EmMgr+8 (usa : voisin 802DE700 ; eur : voisin 802DE700)
    {0x802DFA1Cu, 0x8027F02Cu, 0x8028426Cu},  // EvtMgr
    {0x802DFDD8u, 0x8027F2D8u, 0x80284518u},  // Fade
    {0x802DFF78u, 0x8027F488u, 0x802846C8u},  // Game (usa : voisin 802DFE98 ; eur : voisin 802DFE98)
    {0x802E020Cu, 0x8027F5DCu, 0x8028481Cu},  // IdSys
    {0x802E0478u, 0x8027F7D4u, 0x80284A14u},  // Global (usa : voisin 802E0464 ; eur : voisin 802E0464)
    {0x802E8B30u, 0x80287E90u, 0x8028D0D0u},  // Joy
    {0x802E94D0u, 0x80288830u, 0x8028DA70u},  // Key
    {0x802E982Cu, 0x80288B0Cu, 0x8028DDB4u},  // ModInfoMgr
    {0x802EB044u, 0x8028A28Cu, 0x8028F52Cu},  // Task[0].func (usa : objet 802EAD08 +0x33C ; eur : objet 802EAD08 +0x33C)
    {0x802EE858u, 0x8028DAA0u, 0x80292D40u},  // cSofdec.flag (usa : voisin 802EE828 ; eur : voisin 802EE818)
    {0x80314598u, 0x802B1D20u, 0x802B6FC8u},  // camera fovy (debug)
    {0x8031459Cu, 0x802B1D24u, 0x802B6FCCu},  // camera aspect (usa : voisin 80314598 ; eur : voisin 80314598)
    {0x80314BC8u, 0x802B2280u, 0x802B7548u},  // pG
    {0x80314BCCu, 0x802B2284u, 0x802B754Cu},  // pSys
    {0x80314D5Cu, 0x802B23E4u, 0x802B76ACu},  // repCtr (eur : voisin 80314D44)
    {0x803159F4u, 0x802B3044u, 0x802B8344u},  // pPL
    {0x80315A08u, 0x802B3058u, 0x802B835Cu},  // mess_ptr_buff (usa : voisin 80315A04 ; eur : voisin 80315A04)
    {0x80315AD4u, 0x802B3100u, 0x802B8404u},  // pRK (eur : r13 par usa x5, carte 802B754C)
    {0x80315B30u, 0x802B3144u, 0x802B8448u},  // lockCtr
    {0x8031BEE0u, 0x802B9780u, 0x802BEA20u},  // _SDA_BASE_ (r13)
    {0x803540E0u, 0x00000000u, 0x00000000u},  // file d attente (trace) (usa : inconnue ; eur : inconnue)
