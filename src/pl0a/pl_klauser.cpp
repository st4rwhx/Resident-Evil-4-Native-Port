// pl0a module, second object (D:/Bio4/Prog/pl_klauser.cpp): Krauser: the Leon model set plus the three
// fading arm / mutation models (transMove), the X-button attack routine (pl_R1_KlauserAttack) and the
// tex-render material of the mutation model.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "pl_mod.h"
#include "db_log.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "joy.h"
#include "motion.h"
#include "TexRender.h"

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

static void pl_R1_KlauserAttack(cPlayer* pl);

// The tex-render manager pointer is a one-member struct: every store through it reloads it (r10c idiom).
struct TexRenderMngPtr {
    TexRenderMng* p;
};
static TexRenderMngPtr pl0aTex;
static u8 pl0aTexTbl[0x20];
static f32 pl0aAlphaBase = 80.0f;
asm(".section .data\n\t.balign 8\n\t.text");   // the module's .data is 8-aligned before the BSS tag

extern "C" void setTexRender(cModelInfo* info)
{
    u8* tbl = pl0aTexTbl;

    if (GetTexRenderMgr(&pl0aTex.p)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = pl0aTex.p->texId;
        pl0aTex.p->repType = 1;
        EstSet(0, -1, 0, 0, 3, 0xC, pl0aTex.p->mask | 1, 0, 0, 0);
    } else {
        pLog->err(0, 0, "SetTexRender() : Manager alloc failed!!");
    }
    info->setTexBlendTbl(tbl);
    info->setBlendRatio(0xFF);
    info->setBlendType(1);
}

cPlKlauser::cPlKlauser()
{
    init0();
    EspDataLoad((u32) PL_ARC(0x1A), 3, 0);
    setModel();
    weaponRelease();
    weaponLoad(pG->wep_no, pG->wep_type);
    weaponInit();
    init1();
    setMotion();
    startUp();
    x880 = 1.0f;
    krX7B8 = 0;
    krX7C0 = 0;
    x890 = 0;
    x894 = 1;
    pGS->flags_5018 &= ~0x00800000;
    pFootShadowTbl = pl_fs_tbl;
}

void cPlKlauser::setMotion()
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

void cPlKlauser::move()
{
    cPlayer::move();
    if (x894 > 0) {
        x894--;
        if (x894 == 0) {
            EstSet((int) this, -1, 0, 0, 3, 0, 0, 0x3F, (u32) this, 0);
            EstSet((int) this, -1, 0, 0, 3, 0x15, 0, 0x3F, (u32) this, 0);
        }
    }
    if ((Joy[0].on & 0x640) == 0x640) {
        EffectEspDelete(0, 0x3F, (u32) this, 0);
        EffectEspgenDelete(0, 0x3F, (int) this);
        EffectEfmDelete(0, 0x3F, (int) this);
        x894 = 1;
    }
}

void cPlKlauser::moveMatCalcBefore()
{
    transMove();
}

// Fades a model info's alpha byte in (+0x40 up to 0xFF): a store per arm (jump2 merges them; a single
// store of a result variable lets jump1 hoist the constant arm above the compare).
static inline void alphaUp(cModelInfo* m)
{
    int a = m->color[3];

    if (a <= 0xBE) {
        m->color[3] = a + 0x40;
    } else {
        m->color[3] = 0xFF;
    }
}

// Fades it out by `step` (halved above 0x80); written out in both arms (a helper taking `step`
// by reference puts it in a stack slot).
#define ALPHA_DOWN(m, step)                     \
    {                                           \
        int v;                                  \
        if ((m)->color[3] > 0x80) {             \
            step >>= 1;                         \
        }                                       \
        if ((m)->color[3] > step) {             \
            (m)->color[3] = (m)->color[3] - step; \
        } else {                                \
            (m)->color[3] = 0;                  \
        }                                       \
    }

// be_flag bit3 (draw) follows the alpha byte.
static inline void alphaFlag(cModelInfo* m)
{
    if (m->color[3]) {
        m->be_flag |= 8;
    } else {
        m->be_flag &= ~8;
    }
}

void cPlKlauser::transMove()
{
    int step = 0x40;

    if (pG->flags_5018 & 0x00800000) {
        alphaUp(krModel[1]);
        ALPHA_DOWN(krModel[0], step);
    } else {
        alphaUp(krModel[0]);
        ALPHA_DOWN(krModel[1], step);
    }
    alphaFlag(krModel[0]);
    alphaFlag(krModel[1]);
    if (x894 != 0) {
        if (krModel[2]->color[3] > step) {
            krModel[2]->color[3] = krModel[2]->color[3] - step;
        } else {
            krModel[2]->color[3] = 0;
        }
    } else {
        int a;
        f32 p;
        f32 v;

        a = krModel[2]->color[3];
        if (a < 0xFF - step) {
            krModel[2]->color[3] = a + step;
        } else {
            krModel[2]->color[3] = 0xFF;
        }
        if (x898 <= 0xF) {
            p = (f32) krModel[2]->color[3] * (f32) (x898 + 1) * 0.0625f;
        } else {
            p = (f32) krModel[2]->color[3] * (f32) (0x20 - x898) * 0.0625f;
        }
        v = p * (256.0f - pl0aAlphaBase) * 0.00390625f + pl0aAlphaBase;
        if (v > 255.0f) {
            v = 255.0f;
        }
        krModel[2]->color[3] = (int) v;
        x898++;
        if (x898 > 0x1F) {
            x898 = 0;
        }
    }
    alphaFlag(krModel[2]);
}

int cPlKlauser::checkXbutton()
{
    if ((Joy[0].trg & 0x400) && !(pG->flags_5018 & 0x00800000) && x894 == 0) {
        pAuxFunc = pl_R1_KlauserAttack;
        xFC = 0;
        xFD = 0xA;
        xFE = 0;
        xFF = 0;
        x894 = -1;
        return 1;
    }
    return 0;
}

void cPlKlauser::setModel()
{
    cModelInfo* info;
    cModelInfo* face;

    info = (cModelInfo*) modelInit(PL_ARC(4), PL_ARC(5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlKlauser::setModel() failed.");
        return;
    }
    info = ModInfoMgr.create(PL_ARC(6), PL_ARC(7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlKlauser::setModel() failed.");
        return;
    }
    addModel(info);
    PSet(pBody->pShape, info);
    info = ModInfoMgr.create(PL_ARC(8), PL_ARC(9));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlKlauser::setModel() failed.");
        return;
    }
    krModel[0] = info;
    info->be_flag |= 0x20;
    addModel(info);
    info = ModInfoMgr.create(PL_ARC(0xA), PL_ARC(0xB));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlKlauser::setModel() failed.");
        return;
    }
    krModel[1] = info;
    info->color[3] = 0;
    info->be_flag |= 0x20;
    info->be_flag &= ~8;
    addModel(info);
    info = ModInfoMgr.create(PL_ARC(0xE), PL_ARC(9));
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
    info = ModInfoMgr.create(PL_ARC(0xF), PL_ARC(0x10));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    info = ModInfoMgr.create(PL_ARC(0x18), PL_ARC(0x19));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    krModel[2] = info;
    info->be_flag &= ~8;
    krModel[2]->color[3] = 0;
    krModel[2]->xD8 = 0.9999f;
    setTexRender(info);
    x12D = 1;
    setFace(0);
    setRightHand(0);
    setLeftHand(1);
}

void cPlKlauser::setRightHand(int no)
{
    cModelInfo* info;
    void* data;

    if (pBody->pRight) {
        deleteModelInfo(pBody->pRight);
        pBody->pRight = 0;
        pBody->pRightData = 0;
    }
    switch ((u32) no) {
    case 0:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
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
#line 586 "D:/Bio4/Prog/pl_klauser.cpp"
        HALT();
    }
}

void cPlKlauser::setLeftHand(u32 no)
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
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 8:
    case 9:
        data = PL_ARC(0x14);
        break;
    case 7:
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
        pLog->err(0, 0, "cPlKlauser::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        pBody->pLeft = info;
        pBody->pLeftData = data;
    }
}

void cPlKlauser::setFace(int no)
{
}

void cPlKlauser::setHead(int no)
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
    info = ModInfoMgr.create(PL_ARC_PTR(pGS->pPlArc, 0xC), PL_ARC_PTR(pGS->pPlArc, 7));
    if (info) {
        addModel(info);
    }
}

void cPlKlauser::setHead(void* bin, void* tpl)
{
    cModelInfo* info;

    if (pBody->pShape == 0) {
        return;
    }
    deleteModelInfo(pBody->pShape);
    pBody->pShape = 0;
    info = ModInfoMgr.create(bin, tpl);
    if (info) {
        addModel(info);
    }
}

// Routine 1 / 0xA (cPlayer::pAuxFunc): the X-button mutation attack.
static void pl_R1_KlauserAttack(cPlayer* pl)
{
    const f32 hitLen = 3000.0f;   // pool order: the case-0x15 constant comes first

    switch (pl->xFE) {
    case 0:
        pl->motionSet(PL_ARC(0x8A), 5, 0, 1, 0);
        pl->x890 = 10;
        BitOn(pGS->flags_5018, 0x00800000);
        pl->pNeck->motL = 0;
        DmgMgr.set(3, 0x1E, &pl->pos, 1000.0f, 2000.0f);
        EffectEspDelete(0, 0x3F, (u32) pl, 0);
        EffectEspgenDelete(0, 0x3F, (int) pl);
        EffectEfmDelete(0, 0x3F, (int) pl);
        EstSet((int) pl, -1, 0, 0, 3, 0xA, 0, 0x3F, (u32) pl, 0);
        SndCall(1, 0x51, &pl->pos, 0, 0, 0);
        SndCall(1, 0x52, &pl->pos, 0, 0, 0);
        pl->pWep->pObj->setDisp(1, 0);
        pl->xFE = 1;
        // fallthrough
    case 1:
        if (pl->motionMove()) {
            pl->xFE = 0xA;
        }
        break;
    case 0xA:
        pl->motionSet(PL_ARC(0x8B), 5, 0, 1, 0);
        pl->xFE = 0xB;
        break;
    case 0xB:
        if (Key.on & 4) {
            pl->rot.y -= cPlayer::SPEED_WALK_TURN;
        }
        if (Key.on & 8) {
            pl->rot.y += cPlayer::SPEED_WALK_TURN;
        }
        pl->motionMove();
        if (Joy[0].trg & 0x100) {
            pl->xFE = 0x14;
        } else if (Joy[0].trg & 0x600) {
            pl->xFE = 0x1E;
        }
        break;
    case 0x14:
        pl->motionSet(PL_ARC(0x8C), 5, 0, 1, 0);
        pl->atari.setPriority(2);
        pl->dmg.set(0, 0x80);
        EstSet((int) pl, -1, 0, 0, 3, 0xB, 0, 0x3F, (u32) pl, 0);
        SndCall(1, 0x53, &pl->pos, 0, 0, 0);
        pl->xFE = 0x15;
        // fallthrough
    case 0x15:
        if (pl->frame <= 15.0f) {
            PlWepHitCheck2(0, &pl->pos, &pl->pos, 0x2D, 0, hitLen);
        }
        if (MotionCheckCrossFrame(&pl->mot, 30.0f)) {
            pl->x890 = 0x14;
            BitOff(pGS->flags_5018, 0x00800000);
        }
        if (pl->motionMove()) {
            pl->dmg.clear();
            pl->pWep->pObj->setDisp(1, 1);
            pl->atari.setPriority(0);
            EffectEspDelete(0, 0x3F, (u32) pl, 0);
            EffectEspgenDelete(0, 0x3F, (int) pl);
            EffectEfmDelete(0, 0x3F, (int) pl);
            pl->x894 = 0x546;
            pl->xFC = 0;
            pl->xFD = 0;
            pl->xFE = 0;
            pl->xFF = 0;
        }
        break;
    case 0x1E:
        pl->motionSet(PL_ARC(0x89), 5, 0, 1, 0);
        pl->x890 = 0x14;
        BitOff(pGS->flags_5018, 0x00800000);
        pl->x894 = 1;
        EffectEspDelete(0, 0x3F, (u32) pl, 0);
        EffectEspgenDelete(0, 0x3F, (int) pl);
        EffectEfmDelete(0, 0x3F, (int) pl);
        SndCall(1, 0x52, &pl->pos, 0, 0, 0);
        pl->pWep->pObj->setDisp(1, 1);
        pl->xFE = 0x1F;
        // fallthrough
    case 0x1F:
        if (pl->motionMove()) {
            pl->xFC = 0;
            pl->xFD = 0;
            pl->xFE = 0;
            pl->xFF = 0;
        }
        break;
    }
}

void Pl0aInit(cEm* em)
{
    new (em) cPlKlauser();
}

extern "C" void _prolog()
{
    PlInitFunc = Pl0aInit;
    OSReport("Pl0a Klauser prolog Ok\n");
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}
