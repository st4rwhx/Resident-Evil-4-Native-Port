// game/pl_ashley.cpp: Ashley player class: model set (body, hands, head), motion table, bust motion, cloth.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "joy.h"
#include "pl_cloth.h"
#include "math_sub.h"

extern "C" {
void OSReport(const char* fmt, ...);
void EspDataLoad(void* data, int a, int b);     // game/eff_sys.cpp
void ReleaseWepData();                          // game/read.cpp
f32 sinf(f32 x);
}

extern u8 pl_fs_tbl[];   // game/foot_shadow_tbl.cpp (incomplete type: full address, not @sda21)

#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// Store through a reference: a scalar (non-struct) MEM, so pG is reloaded after every store.
static inline void PSet(void*& d, void* v) { d = v; }
static inline void PSet(cModelInfo*& d, cModelInfo* v) { d = v; }

cPlAshley::cPlAshley()
{
    init0();
    setModel();
    matUpdate();
    bustBase[0] = getPartsPtr(0x1D)->pos;
    bustBase[1] = getPartsPtr(0x1E)->pos;
    bustBase[2] = getPartsPtr(0x1A)->pos;
    pl01weaponSet(this);
    ReleaseWepData();
    init1();
    EspDataLoad(PL_ARC_PTR(pG->pPlArc, 0x1A), 3, 0);
    startUp();
    pFootShadowTbl = pl_fs_tbl;
}

void cPlAshley::move()
{
    cPlayer::move();
    moveBust();
}

void cPlAshley::moveMatCalcBefore()
{
}

void cPlAshley::setModel()
{
    cModelInfo* info;

    info = (cModelInfo*) modelInit(PL_ARC_PTR(pG->pPlArc, 4), PL_ARC_PTR(pG->pPlArc, 5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlAshley::setModel() failed.");
        return;
    }
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlArc, 7), PL_ARC_PTR(pG->pPlArc, 9));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlAshley::setModel() failed.");
        return;
    }
    addModel(info);
    PSet(pBody->pShape, info);
    PSet(pBody->pHeadData, PL_ARC_PTR(pG->pPlArc, 7));
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlArc, 6), PL_ARC_PTR(pG->pPlArc, 0xB));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlAshley::setModel() failed.");
        return;
    }
    addModel(info);
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlArc, 8), PL_ARC_PTR(pG->pPlArc, 5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlAshley::setModel() failed.");
        return;
    }
    info->be_flag |= 0x40;
    addModel(info);
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlArc, 0xA), PL_ARC_PTR(pG->pPlArc, 5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlAshley::setModel() failed.");
        return;
    }
    addModel(info);
    x12D = 1;
    setFace(0);
    setRightHand(0);
    setLeftHand(0);
}

void pl01weaponSet(cPlayer* pl)
{
    int i;

    for (i = 0; i < 0x6D; i++) {
        pl->pMotTbl[i] = 0;
    }
    if ((G_ROOM_ID32 & 0xFFFF0000) == 0x020E0000) {
        PSet(pl->pMotTbl[0x00], PL_ARC_PTR(pG->pPlArc, 0x80));
        PSet(pl->pMotTbl[0x02], PL_ARC_PTR(pG->pPlArc, 0x81));
        PSet(pl->pMotTbl[0x03], PL_ARC_PTR(pG->pPlArc, 0x9A));
        PSet(pl->pMotTbl[0x06], PL_ARC_PTR(pG->pPlArc, 0x83));
        PSet(pl->pMotTbl[0x07], PL_ARC_PTR(pG->pPlArc, 0x9C));
        PSet(pl->pMotTbl[0x08], PL_ARC_PTR(pG->pPlArc, 0x82));
        PSet(pl->pMotTbl[0x09], PL_ARC_PTR(pG->pPlArc, 0x9B));
        PSet(pl->pMotTbl[0x0B], PL_ARC_PTR(pG->pPlArc, 0x84));
        PSet(pl->pMotTbl[0x0C], PL_ARC_PTR(pG->pPlArc, 0x9D));
        PSet(pl->pMotTbl[0x0D], PL_ARC_PTR(pG->pPlArc, 0x86));
        PSet(pl->pMotTbl[0x0E], PL_ARC_PTR(pG->pPlArc, 0x9F));
        PSet(pl->pMotTbl[0x0F], PL_ARC_PTR(pG->pPlArc, 0x85));
        PSet(pl->pMotTbl[0x10], PL_ARC_PTR(pG->pPlArc, 0x9E));
        PSet(pl->pMotTbl[0x3F], PL_ARC_PTR(pG->pPlArc, 0x8E));
        PSet(pl->pMotTbl[0x40], PL_ARC_PTR(pG->pPlArc, 0x8F));
        PSet(pl->pMotTbl[0x39], PL_ARC_PTR(pG->pPlArc, 0x90));
        PSet(pl->pMotTbl[0x3A], PL_ARC_PTR(pG->pPlArc, 0x91));
        PSet(pl->pMotTbl[0x41], PL_ARC_PTR(pG->pPlArc, 0x92));
        PSet(pl->pMotTbl[0x42], PL_ARC_PTR(pG->pPlArc, 0x93));
        PSet(pl->pMotTbl[0x5F], PL_ARC_PTR(pG->pPlArc, 0x87));
        PSet(pl->pMotTbl[0x60], PL_ARC_PTR(pG->pPlArc, 0xA0));
        PSet(pl->pMotTbl[0x61], PL_ARC_PTR(pG->pPlArc, 0x88));
        PSet(pl->pMotTbl[0x62], PL_ARC_PTR(pG->pPlArc, 0xA1));
        PSet(pl->pMotTbl[0x63], PL_ARC_PTR(pG->pPlArc, 0x89));
        PSet(pl->pMotTbl[0x64], PL_ARC_PTR(pG->pPlArc, 0xA2));
        PSet(pl->pMotTbl[0x65], PL_ARC_PTR(pG->pPlArc, 0x8A));
        PSet(pl->pMotTbl[0x66], PL_ARC_PTR(pG->pPlArc, 0xA3));
        PSet(pl->pMotTbl[0x6B], PL_ARC_PTR(pG->pPlArc, 0x8B));
        PSet(pl->pMotTbl[0x6C], PL_ARC_PTR(pG->pPlArc, 0xA4));
        PSet(pl->pMotTbl[0x67], PL_ARC_PTR(pG->pPlArc, 0x8D));
        PSet(pl->pMotTbl[0x68], PL_ARC_PTR(pG->pPlArc, 0xA6));
        PSet(pl->pMotTbl[0x69], PL_ARC_PTR(pG->pPlArc, 0x8C));
        PSet(pl->pMotTbl[0x6A], PL_ARC_PTR(pG->pPlArc, 0xA5));
    } else {
        PSet(pl->pMotTbl[0x00], PL_ARC_PTR(pG->pPlArc, 0x6A));
        PSet(pl->pMotTbl[0x02], PL_ARC_PTR(pG->pPlArc, 0x6B));
        PSet(pl->pMotTbl[0x03], PL_ARC_PTR(pG->pPlArc, 0x6C));
        PSet(pl->pMotTbl[0x06], PL_ARC_PTR(pG->pPlArc, 0x6F));
        PSet(pl->pMotTbl[0x07], PL_ARC_PTR(pG->pPlArc, 0x70));
        PSet(pl->pMotTbl[0x08], PL_ARC_PTR(pG->pPlArc, 0x6D));
        PSet(pl->pMotTbl[0x09], PL_ARC_PTR(pG->pPlArc, 0x6E));
        PSet(pl->pMotTbl[0x0B], PL_ARC_PTR(pG->pPlArc, 0x71));
        PSet(pl->pMotTbl[0x0C], PL_ARC_PTR(pG->pPlArc, 0x72));
        PSet(pl->pMotTbl[0x0D], PL_ARC_PTR(pG->pPlArc, 0x73));
        PSet(pl->pMotTbl[0x0E], PL_ARC_PTR(pG->pPlArc, 0x78));
        PSet(pl->pMotTbl[0x0F], PL_ARC_PTR(pG->pPlArc, 0x74));
        PSet(pl->pMotTbl[0x10], PL_ARC_PTR(pG->pPlArc, 0x79));
        PSet(pl->pMotTbl[0x3F], PL_ARC_PTR(pG->pPlArc, 0x7A));
        PSet(pl->pMotTbl[0x40], PL_ARC_PTR(pG->pPlArc, 0x7B));
        PSet(pl->pMotTbl[0x39], PL_ARC_PTR(pG->pPlArc, 0x7C));
        PSet(pl->pMotTbl[0x3A], PL_ARC_PTR(pG->pPlArc, 0x7D));
        PSet(pl->pMotTbl[0x41], PL_ARC_PTR(pG->pPlArc, 0x7E));
        PSet(pl->pMotTbl[0x42], PL_ARC_PTR(pG->pPlArc, 0x7F));
        PSet(pl->pMotTbl[0x5F], PL_ARC_PTR(pG->pPlArc, 0x32));
        PSet(pl->pMotTbl[0x60], PL_ARC_PTR(pG->pPlArc, 0x33));
        PSet(pl->pMotTbl[0x61], PL_ARC_PTR(pG->pPlArc, 0x34));
        PSet(pl->pMotTbl[0x62], PL_ARC_PTR(pG->pPlArc, 0x35));
        PSet(pl->pMotTbl[0x63], PL_ARC_PTR(pG->pPlArc, 0x36));
        PSet(pl->pMotTbl[0x64], PL_ARC_PTR(pG->pPlArc, 0x37));
        PSet(pl->pMotTbl[0x65], PL_ARC_PTR(pG->pPlArc, 0x38));
        PSet(pl->pMotTbl[0x66], PL_ARC_PTR(pG->pPlArc, 0x39));
        PSet(pl->pMotTbl[0x6B], PL_ARC_PTR(pG->pPlArc, 0x3A));
        PSet(pl->pMotTbl[0x6C], PL_ARC_PTR(pG->pPlArc, 0x3B));
        PSet(pl->pMotTbl[0x67], PL_ARC_PTR(pG->pPlArc, 0x3C));
        PSet(pl->pMotTbl[0x68], PL_ARC_PTR(pG->pPlArc, 0x3D));
        PSet(pl->pMotTbl[0x69], PL_ARC_PTR(pG->pPlArc, 0x3E));
        PSet(pl->pMotTbl[0x6A], PL_ARC_PTR(pG->pPlArc, 0x3F));
    }
}

void cPlAshley::setRightHand(int no)
{
    cModelInfo* info;
    cModelInfo* data;
    void* tpl;

    if (pBody->pRight) {
        deleteModelInfo(pBody->pRight);
        data = pBody->pRight;
        ModInfoMgr.destroy(data);
        pBody->pRight = 0;
        pBody->pRightData = 0;
    }
    switch (no) {
    case 0:
    default:
        if ((G_ROOM_ID32 & 0xFFFF0000) == 0x020E0000) {
            data = (cModelInfo*) PL_ARC_PTR(pG->pPlArc, 0xA7);
            tpl = PL_ARC_PTR(pG->pPlArc, 0xA8);
        } else {
            data = (cModelInfo*) PL_ARC_PTR(pG->pPlArc, 0x11);
            tpl = PL_ARC_PTR(pG->pPlArc, 5);
        }
        break;
    case 1:
        data = (cModelInfo*) pBody->pWepHand;
        tpl = PL_ARC_PTR(pG->pPlArc, 5);
        break;
    }
    if ((info = ModInfoMgr.create(data, tpl)) != 0) {
        addModel(info);
        pBody->pRight = info;
        pBody->pRightData = data;
    }
    if (!info) {
#line 390 "D:/Bio4/Prog/pl_ashley.cpp"
        HALT();
    }
}

void cPlAshley::setLeftHand(u32 no)
{
    cModelInfo* info;
    void* data;

    if (pBody->pLeft) {
        deleteModelInfo(pBody->pLeft);
        ModInfoMgr.destroy(pBody->pLeft);
        pBody->pLeft = 0;
        pBody->pLeftData = 0;
    }
    if (no == 0x63) {
        no = pBody->leftNoPrev;
    }
    switch (no) {
    case 0:
    default:
        data = PL_ARC_PTR(pG->pPlArc, 0x14);
        break;
    case 1:
        data = PL_ARC_PTR(pG->pPlArc, 0x15);
        break;
    }
    pBody->leftNoPrev = pBody->leftNo;
    pBody->leftNo = no;
    info = ModInfoMgr.create(data, PL_ARC_PTR(pGS->pPlArc, 5));
    if (info == 0) {
        pLog->err(0, 0, "cLeon::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        pBody->pLeft = info;
        pBody->pLeftData = data;
    }
}

void cPlAshley::setFace(int no)
{
}

void cPlAshley::moveBust()
{
    static u8 bbx = 0;
    static f32 bul = 0.0f;
    f32 max = 6.0f;
    f32 div = 11.0f;
    Vec ofs;
    cModel* parts;
    cModel* body = getPartsPtr(0);

    if (GetDistance3(&body->worldPos, &body->x88) > 5.0f) {
        bul = max;
    }
    if (Joy[1].on & JOY_X) {
        bul = max;
    } else {
        if (bul > max / div) {
            bul -= max / div;
        } else {
            bul = 0.0f;
        }
    }
    ofs.x = 0.0f;
    ofs.y = sinf((f32) bbx * (PI * 2.0f) * (1.0f / 256.0f)) * bul;
    ofs.z = 0.0f;
    parts = getPartsPtr(0x1D);
    PSVECAdd(&bustBase[0], &ofs, &parts->pos);
    parts->matUpdate();
    PSMTXConcat(parts->pParent->mat, parts->mat, parts->mat);
    parts->worldPos.x = parts->mat[0][3];
    parts->worldPos.y = parts->mat[1][3];
    parts->worldPos.z = parts->mat[2][3];
    parts = getPartsPtr(0x1E);
    PSVECAdd(&bustBase[1], &ofs, &parts->pos);
    parts = getPartsPtr(0x1A);
    PSVECAdd(&bustBase[2], &ofs, &parts->pos);
    bbx += 15;
    parts->matUpdate();
    PSMTXConcat(parts->pParent->mat, parts->mat, parts->mat);
    parts->worldPos.x = parts->mat[0][3];
    parts->worldPos.y = parts->mat[1][3];
    parts->worldPos.z = parts->mat[2][3];
}
