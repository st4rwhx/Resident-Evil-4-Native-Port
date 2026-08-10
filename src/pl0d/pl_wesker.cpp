// pl0d module, third object (D:/Bio4/Prog/pl_wesker.cpp): Wesker: the jacket cloth chain (allocated on
// initCloth), the player class with Leon's motion table and model set (body, hair, head, face shapes).

#include "atari.h"
#include "light.h"
#include "pl_mod.h"
#include "pendulum.h"
#include "db_log.h"
#include "esp.h"
#include "main_mem.h"

extern "C" void OSReport(const char* fmt, ...);

// Plain block, not do/while(0) (pl_leon.cpp).
#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// Store through a reference: a scalar (non-struct) MEM, so pG is reloaded after every store.
static inline void PSet(void*& d, void* v) { d = v; }
static inline void PSet(cModelInfo*& d, cModelInfo* v) { d = v; }

// weskerJacket (the chain tables; the collision volumes are a global: REL field A = 0)
static u8 weskerJacketP[24] = {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87};
static u8 weskerJacketLp[24] = {66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 0xFF, 0xFF};
static u8 weskerJacketUp[24] = {0xFF, 64, 0xFF, 66, 0xFF, 68, 0xFF, 70, 0xFF, 72, 0xFF, 74, 0xFF, 76, 0xFF, 78, 0xFF, 80, 0xFF, 82, 0xFF, 84, 0xFF, 86};
static u8 weskerJacketDp[24] = {65, 0xFF, 67, 0xFF, 69, 0xFF, 71, 0xFF, 73, 0xFF, 75, 0xFF, 77, 0xFF, 79, 0xFF, 81, 0xFF, 83, 0xFF, 85, 0xFF, 87, 0xFF};
static f32 weskerJacketMax[24] = {0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f};
static f32 weskerJacketWindS[24] = {0.0f, 0.0f, 0.4f, 0.4f, 0.9f, 0.9f, 1.2f, 1.2f, 1.5f, 1.5f, 1.7f, 1.7f, 1.9f, 1.9f, 2.1f, 2.1f, 2.4f, 2.4f, 2.8f, 2.8f, 3.1f, 3.1f, -2.8f, -2.8f};
static f32 weskerJacketWindR[24] = {0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f};
PlClothAt weskerJacketAt[6] = {
    {0x0000, 0x11, 0x11, 1.0f, 130.0f, {-30.0f, 0.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x11, 1.0f, 130.0f, {30.0f, 0.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x12, 0.4f, 125.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x16, 0.4f, 125.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 120.0f, {30.0f, -50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 120.0f, {-30.0f, -50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};

static PlCloth* weskerJacket;

void testJacketSetWesker(cModel* pl, PlCloth* c)
{
    f32 rate;

    c->num = 24;
    c->pParts = weskerJacketP;
    c->pLeft = weskerJacketLp;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = weskerJacketUp;
    c->pDown = weskerJacketDp;
    c->x20 = 0;
    c->pRate = 0;
    c->pMax = weskerJacketMax;
    c->pWindS = weskerJacketWindS;
    c->pWindR = weskerJacketWindR;
    c->pAt = weskerJacketAt;
    c->nAt = 6;
    c->x3C = 25.0f;
    rate = 0.5f;
    c->x44 = 4;
    c->x48 = 0.0f;
    c->x4C = 0.1f;
    c->pModel = 0;
    c->x40 = rate;
    c->x50 = rate;
    c->flags = 0x100;
    c->x54 = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

void testJacketMoveWesker(cModel* pl, PlCloth* c)
{
    PenClothMove3(pl, (PenCloth*) c);
}

void PlClothSetWesker(cModel* pl, PlCloth* jacket)
{
    testJacketSetWesker(pl, jacket);
}

void PlClothMoveWesker(cModel* pl, PlCloth* jacket)
{
    testJacketMoveWesker(pl, jacket);
    pl->be_flag &= ~0x00E00000;
}

cPlWesker::cPlWesker()
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

void cPlWesker::setMotion()
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

void cPlWesker::move()
{
    cPlayer::move();
}

void cPlWesker::setModel()
{
    cModelInfo* info;

    info = (cModelInfo*) modelInit(PL_ARC(4), PL_ARC(5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlWesker::setModel() failed.");
        return;
    }
    info = ModInfoMgr.create(PL_ARC(0xA), PL_ARC(5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlWesker::setModel() failed.");
        return;
    }
    addModel(info);
    info = ModInfoMgr.create(PL_ARC(8), PL_ARC(7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlWesker::setModel() failed.");
        return;
    }
    addModel(info);
    PSet(pBody->pShape, info);
    PSet(pBody->pHeadData, PL_ARC(8));
    info = ModInfoMgr.create(PL_ARC(6), PL_ARC(7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlWesker::setModel() failed.");
        return;
    }
    addModel(info);
    pBody->pHair = info;
    x12D = 1;
    setFace(0);
    setRightHand(0);
    setLeftHand(0);
}

void cPlWesker::setRightHand(int no)
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
#line 515 "D:/Bio4/Prog/pl_wesker.cpp"
        HALT();
    }
}

void cPlWesker::setLeftHand(u32 no)
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
    case 2:
        data = PL_ARC(0x16);
        break;
    case 4:
        data = PL_ARC(0x18);
        break;
    default:
        data = (void*) no;
        break;
    }
    pBody->leftNoPrev = pBody->leftNo;
    pBody->leftNo = no;
    info = ModInfoMgr.create(data, PL_ARC_PTR(pGS->pPlArc, 0x11));
    if (info == 0) {
        pLog->err(0, 0, "cPlWesker::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        pBody->pLeft = info;
        pBody->pLeftData = data;
    }
}

void cPlWesker::setFace(int no)
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
        data = PL_ARC(0x62);
        break;
    case 2:
        data = PL_ARC(0x63);
        break;
    }
    if (no != 0) {
        ShapeSet(pBody->pShape, 0, data, 2);
    }
}

void cPlWesker::setHead(int no)
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
    info = ModInfoMgr.create(PL_ARC_PTR(pGS->pPlArc, 0xB), PL_ARC_PTR(pGS->pPlArc, 7));
    if (info) {
        addModel(info);
    }
}

void cPlWesker::setHead(void* bin, void* tpl)
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

void cPlWesker::initCloth()
{
    weskerJacket = (PlCloth*) MemAlloc(sizeof(PlCloth), 1);
    if (weskerJacket) {
        PlClothSetWesker(this, weskerJacket);
    }
}

void cPlWesker::moveCloth()
{
    if (weskerJacket) {
        PlClothMoveWesker(this, weskerJacket);
    }
}

void Pl0dInit(cEm* em)
{
    new (em) cPlWesker();
}

extern "C" void _prolog()
{
    PlInitFunc = Pl0dInit;
    OSReport("Pl0d WESKER prolog Ok\n");
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}
