// pl06 module (D:/Bio4/Prog/pl_hunk.cpp): HUNK, the mercenaries player: model set (body, hair, hands), the
// Leon motion table, no face shapes and no cloth.

#include "atari.h"
#include "light.h"
#include "pl_mod.h"
#include "db_log.h"
#include "esp.h"

extern "C" void OSReport(const char* fmt, ...);

// Plain block, not do/while(0) (pl_leon.cpp).
#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

// Store through a reference: a scalar (non-struct) MEM, so pG is reloaded after every store.
static inline void PSet(void*& d, void* v) { d = v; }

cPlHunk::cPlHunk()
{
    PlArc* arc;

    init0();
    setModel();
    weaponRelease();
    weaponLoad(pG->wep_no, pG->wep_type);
    weaponInit();
    init1();
    setMotion();
    arc = pG->pPlArc;
    EspDataLoad((u32) PL_ARC_PTR(arc, 0x1A), 3, 0);
    startUp();
    pFootShadowTbl = pl_fs_tbl;
}

void cPlHunk::setMotion()
{
    PSet(pMotTbl[0x5F], PL_ARC(0x32));
    PSet(pMotTbl[0x60], PL_ARC(0x33));
    PSet(pMotTbl[0x61], PL_ARC(0x34));
    PSet(pMotTbl[0x62], PL_ARC(0x35));
    PSet(pMotTbl[0x63], PL_ARC(0x36));
    PSet(pMotTbl[0x64], PL_ARC(0x37));
    PSet(pMotTbl[0x65], PL_ARC(0x38));
    PSet(pMotTbl[0x66], PL_ARC(0x39));
    PSet(pMotTbl[0x6B], PL_ARC(0x3A));
    PSet(pMotTbl[0x6C], PL_ARC(0x3B));
    PSet(pMotTbl[0x67], PL_ARC(0x3C));
    PSet(pMotTbl[0x68], PL_ARC(0x3D));
    PSet(pMotTbl[0x69], PL_ARC(0x3E));
    PSet(pMotTbl[0x6A], PL_ARC(0x3F));
}

void cPlHunk::move()
{
    cPlayer::move();
}

void cPlHunk::setModel()
{
    cModelInfo* info;

    if (modelInit(PL_ARC(4), PL_ARC(5)) == 0) {
        pLog->err(0, 0, "cSubLuis::init() failed.");
    }
    if ((info = ModInfoMgr.create(PL_ARC(6), PL_ARC(7))) != 0) {
        addModel(info);
        pBody->pHair = info;
    }
    if ((info = ModInfoMgr.create(PL_ARC(0x12), PL_ARC(0x11))) != 0) {
        addModel(info);
        pBody->pRight = info;
    }
    if ((info = ModInfoMgr.create(PL_ARC(0x14), PL_ARC(0x11))) != 0) {
        addModel(info);
        pBody->pLeft = info;
    }
    x12D = 1;
    setFace(0);
    setRightHand(0);
    setLeftHand(1);
}

void cPlHunk::setRightHand(int no)
{
    cModelInfo* info;
    void* data;

    if (pBody->pRight) {
        deleteModelInfo(pBody->pRight);
        pBody->pRight = 0;
        pBody->pRightData = 0;
    }
    switch (no) {
    case 0:
        data = PL_ARC(0x12);
        break;
    case 1:
        data = pBody->pWepHand;
        break;
    default:
        data = (void*) no;
        break;
    }
    if ((info = ModInfoMgr.create(data, PL_ARC(0x11))) != 0) {
        addModel(info);
        pBody->pRight = info;
        pBody->pRightData = data;
    }
    if (!info) {
#line 253 "D:/Bio4/Prog/pl_hunk.cpp"
        HALT();
    }
}

void cPlHunk::setLeftHand(u32 no)
{
    cModelInfo* info;
    void* data;

    if (pBody->pLeft) {
        deleteModelInfo(pBody->pLeft);
        pBody->pLeft = 0;
        pBody->pLeftData = 0;
    }
    if (no == 0x63) {
        no = pBody->leftNoPrev;
    }
    switch (no) {
    case 0:
        data = PL_ARC(0x14);
        break;
    case 1:
    case 3:
        data = PL_ARC(0x15);
        break;
    default:
        data = (void*) no;
        break;
    }
    pBody->leftNoPrev = pBody->leftNo;
    pBody->leftNo = no;
    info = ModInfoMgr.create(data, PL_ARC_PTR(pGS->pPlArc, 0x11));
    if (info == 0) {
        pLog->err(0, 0, "cPlHunk::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        pBody->pLeft = info;
        pBody->pLeftData = data;
    }
}

void cPlHunk::setFace(int no)
{
}

void cPlHunk::setHead(int no)
{
    cModelInfo* info;

    if (no != 0) {
        return;
    }
    if (pBody->pHair == 0) {
        return;
    }
    deleteModelInfo(pBody->pHair);
    pBody->pHair = 0;
    info = ModInfoMgr.create(PL_ARC_PTR(pGS->pPlArc, 0xB), PL_ARC_PTR(pGS->pPlArc, 7));
    if (info) {
        addModel(info);
    }
}

void cPlHunk::setHead(void* bin, void* tpl)
{
    cModelInfo* info;

    if (pBody->pShape == 0) {
        return;
    }
    deleteModelInfo(pBody->pHair);
    pBody->pHair = 0;
    info = ModInfoMgr.create(bin, tpl);
    if (info) {
        addModel(info);
    }
}

void Pl06Init(cEm* em)
{
    new (em) cPlHunk();
}

extern "C" void _prolog()
{
    PlInitFunc = Pl06Init;
    OSReport("Pl06 HUNK prolog Ok\n");
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}
