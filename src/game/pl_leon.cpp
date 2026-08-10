// game/pl_leon.cpp: Leon player class: model set (body, hands, head, face), motion table, cloth.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "snd.h"

extern "C" {
void OSReport(const char* fmt, ...);
void EspDataLoad(void* data, int a, int b);     // game/eff_sys.cpp
int SubCharCheckCtrl();                         // game/pl_sub.cpp
void SubCharCtrl(int a, int b);                 // game/pl_sub.cpp
}
u32 SubCharGetStatus();                         // game/pl_npc.cpp
void ShapeSet(void* info, int a, void* data, int b);  // game/shape.cpp
void ShapeEnd(void* info);

extern cModel* pSUB;
extern u8 pl_fs_tbl[];   // game/foot_shadow_tbl.cpp (incomplete type: full address, not @sda21)

// Plain block, not do/while(0): the do-while's deleted back-jump lets cse rewrite the HALT store's
// zero as `info` (one more ref), which makes `info` outrank `data` in global allocation order.
#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// Store through a reference: a scalar (non-struct) MEM, so pG is reloaded after every store.
static inline void PSet(void*& d, void* v) { d = v; }
static inline void PSet(cModelInfo*& d, cModelInfo* v) { d = v; }

cPlLeon::cPlLeon()
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
    EspDataLoad(PL_ARC_PTR(arc, 0x1A), 3, 0);
    startUp();
    pFootShadowTbl = pl_fs_tbl;
}

void cPlLeon::setMotion()
{
    PSet(pMotTbl[0x5F], PL_ARC_PTR(pG->pPlArc, 0x32));
    PSet(pMotTbl[0x60], PL_ARC_PTR(pG->pPlArc, 0x33));
    PSet(pMotTbl[0x61], PL_ARC_PTR(pG->pPlArc, 0x34));
    PSet(pMotTbl[0x62], PL_ARC_PTR(pG->pPlArc, 0x35));
    PSet(pMotTbl[0x63], PL_ARC_PTR(pG->pPlArc, 0x36));
    PSet(pMotTbl[0x64], PL_ARC_PTR(pG->pPlArc, 0x37));
    PSet(pMotTbl[0x65], PL_ARC_PTR(pG->pPlArc, 0x38));
    PSet(pMotTbl[0x66], PL_ARC_PTR(pG->pPlArc, 0x39));
    PSet(pMotTbl[0x6B], PL_ARC_PTR(pG->pPlArc, 0x3A));
    PSet(pMotTbl[0x6C], PL_ARC_PTR(pG->pPlArc, 0x3B));
    PSet(pMotTbl[0x67], PL_ARC_PTR(pG->pPlArc, 0x3C));
    PSet(pMotTbl[0x68], PL_ARC_PTR(pG->pPlArc, 0x3D));
    PSet(pMotTbl[0x69], PL_ARC_PTR(pG->pPlArc, 0x3E));
    PSet(pMotTbl[0x6A], PL_ARC_PTR(pG->pPlArc, 0x3F));
}

void cPlLeon::move()
{
    cPlayer::move();
}

void cPlLeon::setModel()
{
    cModelInfo* info;
    cModelInfo* face;

    info = (cModelInfo*) modelInit(PL_ARC_PTR(pG->pPlArc, 4), PL_ARC_PTR(pG->pPlArc, 5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    switch (pG->costume) {
    case 0:
    case 1:
    case 2:
    case 3:
        info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlArc, 0xA), PL_ARC_PTR(pG->pPlArc, 5));
        if (!VALID_PTR(info)) {
            pLog->err(0, 0, "cPlLeon::setModel() failed.");
            return;
        }
        addModel(info);
        break;
    }
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlArc, 0xD), PL_ARC_PTR(pG->pPlArc, 5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    pBody->pFace = info;
    face = pBody->pFace;
    if (VALID_PTR(face)) {
        face->x84 = 0.0f;
        face->x70 = 0.0f;
        face->x5C = 0.0f;
    }
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlArc, 8), PL_ARC_PTR(pG->pPlArc, 7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    PSet(pBody->pShape, info);
    PSet(pBody->pHeadData, PL_ARC_PTR(pG->pPlArc, 8));
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlArc, 6), PL_ARC_PTR(pG->pPlArc, 7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    PSet(pBody->pHair, info);
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlArc, 9), PL_ARC_PTR(pG->pPlArc, 7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    info->be_flag |= 0x40;
    PSet(pBody->pEye, info);
    if (pG->costume >= 1 && pG->costume <= 3) {
        info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlArc, 0x10), PL_ARC_PTR(pG->pPlArc, 5));
        if (!VALID_PTR(info)) {
            pLog->err(0, 0, "cPlLeon::setModel() failed.");
            return;
        }
        addModel(info);
    }
    if (pG->flags_51C0 & 0x20) {
        setWound();
    }
    x12D = 1;
    setFace(0);
    setRightHand(0);
    setLeftHand(1);
}

void cPlLeon::setWound()
{
    cModelInfo* info;

    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlArc, 0xE), PL_ARC_PTR(pG->pPlArc, 0xF));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
    } else {
        addModel(info);
    }
}

void cPlLeon::setRightHand(int no)
{
    void* data;
    cModelInfo* info;

    if (pBody->pRight) {
        deleteModelInfo(pBody->pRight);
        pBody->pRight = 0;
        pBody->pRightData = 0;
    }
    switch (no) {
    case 0:
        data = PL_ARC_PTR(pG->pPlArc, 0x12);
        break;
    case 1:
        data = pBody->pWepHand;
        break;
    default:
        data = (void*) no;
        break;
    }
    if ((info = ModInfoMgr.create((void*) data, PL_ARC_PTR(pG->pPlArc, 0x11))) != 0) {
        addModel(info);
        pBody->pRight = info;
        pBody->pRightData = (void*) data;
    }
    if (!info) {
#line 353 "D:/Bio4/Prog/pl_leon.cpp"
        HALT();
    }
}

void cPlLeon::setLeftHand(u32 no)
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
        data = PL_ARC_PTR(pG->pPlArc, 0x14);
        break;
    case 1:
        data = PL_ARC_PTR(pG->pPlArc, 0x15);
        break;
    case 2:
        data = PL_ARC_PTR(pG->pPlArc, 0x16);
        break;
    case 3:
        data = PL_ARC_PTR(pG->pPlArc, 0x17);
        break;
    case 4:
        data = PL_ARC_PTR(pG->pPlArc, 0x18);
        break;
    case 5:
        data = PL_ARC_PTR(pG->pPlArc, 0x19);
        break;
    default:
        data = (void*) no;
        break;
    }
    pBody->leftNoPrev = pBody->leftNo;
    pBody->leftNo = no;
    info = ModInfoMgr.create(data, PL_ARC_PTR(pGS->pPlArc, 0x11));
    if (info == 0) {
        pLog->err(0, 0, "cPlLeon::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        pBody->pLeft = info;
        pBody->pLeftData = data;
    }
}

void cPlLeon::setFace(int no)
{
    void* data = 0;
    void* shape = pBody->pShape;

    if (shape == 0) {
        return;
    }
    switch (no) {
    case 0:
    default:
        ShapeEnd(shape);
        break;
    case 1:
        data = PL_ARC_PTR(pG->pPlArc, 0x62);
        break;
    case 2:
        data = PL_ARC_PTR(pG->pPlArc, 0x63);
        break;
    }
    if (no != 0) {
        ShapeSet(pBody->pShape, 0, data, 2);
    }
}

void cPlLeon::setHead(int no)
{
    cModelInfo* info;

    if (no != 0) {
        return;
    }
    if (pBody->pShape == 0) {
        return;
    }
    deleteModelInfo(pBody->pShape);
    pBody->pShape = 0;
    deleteModelInfo(pBody->pHair);
    pBody->pHair = 0;
    deleteModelInfo(pBody->pEye);
    pBody->pEye = 0;
    info = ModInfoMgr.create(PL_ARC_PTR(pGS->pPlArc, 0xB), PL_ARC_PTR(pGS->pPlArc, 7));
    if (info) {
        addModel(info);
    }
}

void cPlLeon::setHead(void* bin, void* tpl)
{
    cModelInfo* info;

    if (pBody->pShape == 0) {
        return;
    }
    deleteModelInfo(pBody->pShape);
    pBody->pShape = 0;
    deleteModelInfo(pBody->pHair);
    pBody->pHair = 0;
    deleteModelInfo(pBody->pEye);
    pBody->pEye = 0;
    info = ModInfoMgr.create(bin, tpl);
    if (info) {
        addModel(info);
    }
}

int cPlLeon::checkXbutton()
{
    if (xButtonWait) {
        xButtonWait--;
    }
    if (pSUB == 0) {
        return 0;
    }
    if (pSUB->id != 3) {
        return 0;
    }
    pG->flags_5010 |= 4;
    if (xButtonWait != 0) {
        return 0;
    }
    if (SubCharCheckCtrl() == 0) {
        return 0;
    }
    if (!(Key.trg & 0x200)) {
        return 0;
    }
    if (SubCharGetStatus() & 0x40000000) {
        SndCall(1, 0x37, &pParts->worldPos, 0, 0, 0);
        SubCharCtrl(1, 0);
    } else {
        SndCall(1, 0x36, &pParts->worldPos, 0, 0, 0);
        SubCharCtrl(0, 0);
    }
    xButtonWait = 8;
    pG->flags_500C |= 0x800000;
    return 1;
}
