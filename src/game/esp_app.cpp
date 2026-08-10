#include "atari.h"
#include "light.h"
#include "global.h"
#include "player.h"
#include "snd.h"
#include "area.h"
#include "eprintf.h"
#include "math_sub.h"
#include "TexRender.h"
#include "esp.h"
#include "espgen.h"

// laser line: cEsp19 (game/esp19.cpp) work
struct Esp19Work {
    Vec target;  // 0x00 end point of the line
    f32 len;     // 0x0C maximum length
};
class cEsp19 : public cEsp {
public:
    Esp19Work w;  // 0xF8
};

class cEsp46;

cEsp* Esp00_Create();
cEsp* Esp01_Create();
cEsp* Esp02_Create();
cEsp* Esp03_Create();
cEsp* Esp04_Create();
cEsp* Esp05_Create();
cEsp* Esp06_Create();
cEsp* Esp07_Create();
cEsp* Esp09_Create();
cEsp* Esp0a_Create();
cEsp* Esp0b_Create();
cEsp* Esp0c_Create();
cEsp* Esp0d_Create();
cEsp* Esp0e_Create();
cEsp* Esp0f_Create();
cEsp* Esp10_Create();
cEsp* Esp11_Create();
cEsp* Esp12_Create();
cEsp* Esp13_Create();
cEsp* Esp14_Create();
cEsp* Esp15_Create();
cEsp* Esp16_Create();
cEsp* Esp17_Create();
cEsp* Esp19_Create();
cEsp* Esp1a_Create();
cEsp* Esp1b_Create();
cEsp* Esp3f_Create();
cEsp* Esp40_Create();
cEsp* Esp41_Create();
cEsp* Esp42_Create();
cEsp* Esp43_Create();
cEsp* Esp44_Create();
cEsp* Esp46_Create();
cEsp* Esp47_Create();
cEsp* Esp48_Create();
cEsp* Esp49_Create();
cEsp* Esp4a_Create();
cEsp* Esp4b_Create();
cEsp* Esp4c_Create();
cEsp* Esp4d_Create();
cEsp* Esp4f_Create();
void Esp11_Trans(cEsp* esp);
void Esp46_Trans(cEsp46* esp);
void Esp47_Trans(cEsp* esp);
void Esp4a_Trans();
void Esp4c_Trans();
void Esp4d_Trans();

extern "C" {
cEsp* Esp08_Create();
cEsp* Esp18_Create();
cEsp* Esp45_Create();
cEsp* Esp4e_Create();
void Esp01_Trans(cEsp* esp);
void Esp02_Trans(cEsp* esp);
void Esp03_Trans(cEsp* esp);
void Esp04_Trans(cEsp* esp);
void Esp08_Trans(cEsp* esp);
void Esp09_Trans(cEsp* esp);
void Esp0a_Trans(cEsp* esp);
void Esp0b_Trans(cEsp* esp);
void Esp0c_Trans(cEsp* esp);
void Esp0e_Trans(cEsp* esp);
void Esp0f_Trans(cEsp* esp);
void Esp12_Trans(cEsp* esp);
void Esp16_Trans(cEsp* esp);
void Esp18_Trans(cEsp* esp);
void Esp19_Trans(cEsp* esp);
void Esp45_Trans(cEsp* esp);
void Esp4e_Trans();
}

typedef void (*EffSeFunc)(Vec* pos);

EffSeFunc pSeFunc[8];
// The mask read after the `repType = 1` store goes through a struct-member view of the pointer
// (the pLog trick, db_log.h) so it stays below the store; the other reads use the plain global.
TexRenderMng* g_pMgr;
struct TexRenderMngPtr {
    TexRenderMng* p;
};
#define pMgr g_pMgr
#define pMgrView (((TexRenderMngPtr*) &g_pMgr)->p)
static inline void ISet(int& d, int v) { d = v; }

void EffSetId()
{
    EspFuncTblSet(0x00, Esp00_Create, EspCommonTrans);
    EspFuncTblSet(0x01, Esp01_Create, Esp01_Trans);
    EspFuncTblSet(0x02, Esp02_Create, Esp02_Trans);
    EspFuncTblSet(0x03, Esp03_Create, Esp03_Trans);
    EspFuncTblSet(0x04, Esp04_Create, Esp04_Trans);
    EspFuncTblSet(0x05, Esp05_Create, EspCommonTrans);
    EspFuncTblSet(0x06, Esp06_Create, EspCommonTrans);
    EspFuncTblSet(0x07, Esp07_Create, EspCommonTrans);
    EspFuncTblSet(0x08, Esp08_Create, Esp08_Trans);
    EspFuncTblSet(0x09, Esp09_Create, Esp09_Trans);
    EspFuncTblSet(0x0a, Esp0a_Create, Esp0a_Trans);
    EspFuncTblSet(0x0b, Esp0b_Create, Esp0b_Trans);
    EspFuncTblSet(0x0c, Esp0c_Create, Esp0c_Trans);
    EspFuncTblSet(0x0d, Esp0d_Create, EspCommonTrans);
    EspFuncTblSet(0x0e, Esp0e_Create, Esp0e_Trans);
    EspFuncTblSet(0x0f, Esp0f_Create, Esp0f_Trans);
    EspFuncTblSet(0x10, Esp10_Create, EspCommonTrans);
    EspFuncTblSet(0x11, Esp11_Create, Esp11_Trans);
    EspFuncTblSet(0x12, Esp12_Create, Esp12_Trans);
    EspFuncTblSet(0x13, Esp13_Create, EspCommonTrans);
    EspFuncTblSet(0x14, Esp14_Create, EspCommonTrans);
    EspFuncTblSet(0x15, Esp15_Create, EspCommonTrans);
    EspFuncTblSet(0x16, Esp16_Create, Esp16_Trans);
    EspFuncTblSet(0x17, Esp17_Create, EspCommonTrans);
    EspFuncTblSet(0x18, Esp18_Create, Esp18_Trans);
    EspFuncTblSet(0x19, Esp19_Create, Esp19_Trans);
    EspFuncTblSet(0x1a, Esp1a_Create, EspCommonTrans);
    EspFuncTblSet(0x1b, Esp1b_Create, EspCommonTrans);
    EspFuncTblSet(0x3f, Esp3f_Create, NULL);
    EspFuncTblSet(0x40, Esp40_Create, EspCommonTrans);
    EspFuncTblSet(0x41, Esp41_Create, EspCommonTrans);
    EspFuncTblSet(0x42, Esp42_Create, EspCommonTrans);
    EspFuncTblSet(0x43, Esp43_Create, EspCommonTrans);
    EspFuncTblSet(0x44, Esp44_Create, EspCommonTrans);
    EspFuncTblSet(0x45, Esp45_Create, Esp45_Trans);
    EspFuncTblSet(0x46, Esp46_Create, (EspTransFunc) Esp46_Trans);
    EspFuncTblSet(0x47, Esp47_Create, Esp47_Trans);
    EspFuncTblSet(0x48, Esp48_Create, EspCommonTrans);
    EspFuncTblSet(0x49, Esp49_Create, EspCommonTrans);
    EspFuncTblSet(0x4a, Esp4a_Create, (EspTransFunc) Esp4a_Trans);
    EspFuncTblSet(0x4b, Esp4b_Create, EspCommonTrans);
    EspFuncTblSet(0x4c, Esp4c_Create, (EspTransFunc) Esp4c_Trans);
    EspFuncTblSet(0x4d, Esp4d_Create, (EspTransFunc) Esp4d_Trans);
    EspFuncTblSet(0x4e, Esp4e_Create, (EspTransFunc) Esp4e_Trans);
    EspFuncTblSet(0x4f, Esp4f_Create, EspCommonTrans);
}

void EspFreeSizeCheckAll()
{
}

void EspCallSeType(int type, Vec* pos)
{
    if (EspGenGetMoveLoop()) {
        return;
    }
    switch (type) {
    case 0:
        break;
    case 1:
        SndCall(6, 0x2F, pos, 0, 0, NULL);
        break;
    case 2:
        SndCall(6, 0x0D, pos, 0, 0, NULL);
        break;
    case 3:
        break;
    default:
        pLog->err(0, 0, "EspCallSeType() : SeType[%d] invalid.", type);
        break;
    }
}

void EspFootCall(int type, int no, Vec* pos)
{
    cPlayer* pl = pPL;
    Vec fpos;
    f32 h;

    switch (type) {
    case 0:
        switch (no) {
        case 0:
            break;
        case 1:
            EstSet(0, -1, pos, NULL, 0, 4, 0, 0, 0, NULL);
            break;
        case 2:
            EstSet(0, -1, pos, NULL, 0, 0x16, 0, 0, 0, NULL);
            break;
        case 3:
            EstSet(0, -1, pos, NULL, 0, 4, 0, 0, 0, NULL);
            break;
        case 4:
            h = EatMgr.getFloor(&pl->pos, 600.0f, 100000.0f, NULL, 0);
            fpos.x = pl->pos.x;
            fpos.y = h;
            fpos.z = pl->pos.z;
            EstSet(0, -1, &fpos, NULL, pl->pRoomEff[4].id, pl->pRoomEff[4].type, 0, 0, 0, NULL);
            break;
        default:
            pLog->err(0, 0, "EspFootCall() : FootSeNo[%d] invalid.", no);
            break;
        }
        break;
    case 1:
        switch (no) {
        case 0:
            break;
        case 1:
            EstSet(0, -1, pos, NULL, 0, 5, 0, 0, 0, NULL);
            break;
        case 2:
            EstSet(0, -1, pos, NULL, 0, 0x17, 0, 0, 0, NULL);
            break;
        case 3:
            EstSet(0, -1, pos, NULL, 0, 5, 0, 0, 0, NULL);
            break;
        case 4:
            h = EatMgr.getFloor(&pl->pos, 600.0f, 100000.0f, NULL, 0);
            fpos.x = pl->pos.x;
            fpos.y = h;
            fpos.z = pl->pos.z;
            EstSet(0, -1, &fpos, NULL, pl->pRoomEff[5].id, pl->pRoomEff[5].type, 0, 0, 0, NULL);
            break;
        default:
            pLog->err(0, 0, "EspFootCall() : FootSeNo[%d] invalid.", no);
            break;
        }
        break;
    default:
        pLog->err(0, 0, "EspFootCall() : FootSeType[%d] invalid.", type);
        break;
    }
}

int EspPlWaterCall(int type, Vec* pos)
{
    Vec wpos;
    f32 h;
    int ret = 0;

    if (GetWaterHeight(pos, &h)) {
        if (pos->y < h - 90.0f) {
            AddWaterPower(&pPL->pos, 0.25f);
            ret = 1;
            wpos = *pos;
            wpos.y = h;
        }
    }
    return ret;
}

void EffSetRoomSeFunc(int no, EffSeFunc func)
{
    if (no < 0 || no > 7) {
        pLog->err(0, 0, "EffSetRoomSeFunc() : SeNo[%d] invalid.", no);
        return;
    }
    pSeFunc[no] = func;
}

void EffCrearRoomSeFunc()
{
    EffSeFunc* p = pSeFunc;
    int i;

    for (i = 0; i < 8; i++) {
        *p++ = NULL;
    }
}

void EffCallRoomSeFunc(int no, Vec* pos)
{
    if (no < 0 || no > 7) {
        pLog->err(0, 0, "EffCallRoomSeFunc() : SeNo[%d] invalid.", no);
        return;
    }
    if (pSeFunc[no] == NULL) {
        pLog->err(0, 0, "EffCallRoomSeFunc() : SeFunc[%d] not init.", no);
        return;
    }
    pSeFunc[no](pos);
}

void EffAreaUpdate()
{
    cEspSystem* sys = g_pEspSys;
    Vec pos;
    u32 flag;
    u32 i;
    u32 j;
    int y;
    SstAreaEnt* ent;

    if (pG->flags_5010 & 0x800) {
        BitOn(pG->flags_5010, 0x02000000);
    } else {
        BitOff(pG->flags_5010, 0x02000000);
    }
    if (pG->flags_170 & 0x20) {
        return;
    }
    if (sys->pSstArea == NULL) {
        return;
    }
    if ((pG->flags_5014 & 0x10000) == 0) {
        pos = pPL->pos;
        pos.y += 100.0f;
    } else {
        pos = pG->Cam.param.pos;
    }
    flag = 0;
    ent = sys->pSstArea->ent;
    for (i = 0; i < sys->pSstArea->num; i++, ent++) {
        if (AreaHitCheck(ent->area, &pos) == 1) {
            flag |= 1 << ent->bit;
            if (ent->flags & 1) {
                pG->flags_5010 |= 0x02000000;
            }
        }
    }
    flag |= sys->sstAddAreaFlag;
    asm("" : "=m"(*(u32*) &pos)); // COMPILER-DIFF: candidate (sched2 issue-slot filler)
    y = 0;
    // y is the hit count; the row `0xE8 + y * 0x10` is a strength-reduced giv (its `li 0xE8` is
    // the last preheader insn). The codeless asm above is an issue-slot filler: sched2 (2 insns
    // per cycle) issues it with the `lwz sstAddAreaFlag`, so `li j` and `li 1` take cycle 2 and
    // the hoisted `lis "%d"` is issued after the `or` like the target (the original's block had
    // one more insn there). A scalar frame MEM (`*(u32*) &pos`) has no dependence on the in-struct
    // load; `pos.x` (in-struct) would delay the lwz.
    for (j = 0; j < 32; j++) {
        if (flag & (1 << j)) {
            if (pG->flags_6C & 0x8000) {
                eprintf(0x1D8, 0xE8 + y * 0x10, 0x16, 0, "%d", j);
                y++;
            }
            EffSetAreaState(j, 1);
        } else {
            EffSetAreaState(j, 0);
        }
    }
}

int EffAreaCheckInRoom(Vec* pos)
{
    cEspSystem* sys = g_pEspSys;
    SstAreaEnt* ent;
    u32 i;

    ent = sys->pSstArea->ent;
    for (i = 0; i < sys->pSstArea->num; i++, ent++) {
        if (AreaHitCheck(ent->area, pos) == 1) {
            if (ent->flags & 1) {
                return 1;
            }
        }
    }
    return 0;
}

int EffAreaCheckNo(Vec* pos, u8 areaNo)
{
    cEspSystem* sys = g_pEspSys;
    SstAreaEnt* ent;
    u32 i;

    ent = sys->pSstArea->ent;
    for (i = 0; i < sys->pSstArea->num; i++, ent++) {
        if (areaNo == ent->bit) {
            if (AreaHitCheck(ent->area, pos) == 1) {
                return 1;
            }
        }
    }
    return 0;
}

void EffEm2d_setTexRender(cModel* m)
{
    static u8 buf[0x80];
    u8* tbl = buf;
    int repType = 1;

    if ((pG->flags_5010 & 0x10) == 0) {
        TexRenderMng* mgr;
        TexRenderMng* mgr2;

        if (!GetTexRenderMgr(&pMgr)) {
            pLog->err(0, 0, "EffEm2d_setTexRender() : Manager alloc failed!!");
            return;
        }
        BitOn(pG->flags_5010, 0x10);
        mgr = pMgr;
        BitSet(mgr->sx, 0x40);
        BitSet(mgr->sy, 0x40);
        pMgr->ReAllocBuf();
        tbl[0] = 4;
        tbl[1] = 0;
        tbl[4] = 0;
        mgr2 = pMgr;
        tbl[5] = mgr2->texId;
        tbl[6] = 2;
        tbl[7] = mgr2->texId;
        tbl[8] = 4;
        tbl[9] = mgr2->texId;
        tbl[0xA] = 6;
        tbl[0xB] = mgr2->texId;
        ISet(mgr2->repType, repType);
        EstSet(0, -1, NULL, NULL, 0x25, 0x1F, pMgr->mask | 0x801, 0, 0, NULL);
    }
    m->pInfo->setTexBlendTbl(tbl);
    m->pInfo->setBlendRatio(0);
}

// The five `esp` reloads and the 0.8f pool high are local-alloc qtys allocated by priority
// refs*log2(refs)/life over the sched1 order with +-1-insn fake lifetimes: the high (refs 2,
// life 2 = 10000) goes first and takes r9, flipping every reload (r9,r9,r11,r9,r11 ->
// r11,r11,r9,r11,r9). The codeless asm below mentions reload 1 twice (refs 4, dies one insn
// later: 80000/12 = 6666) and, being output-dependent on `stb a4`, is ready one cycle after
// the `lis` in sched1 and lands between `lis` and `lfs` (the high's life 2 -> 4 = 5000).
// Reload 1 is then allocated before the high (r9), the high falls to r11 and the walk gives
// the target; in sched2 the asm fills the empty slot beside the `lfs` and emits nothing.
void EspDrawLaserLine(Vec from, Vec to, f32 width)
{
    cEsp* esp;
    cEsp19* e;
    Esp19Work* w;

    if (pG->flags_6C & 0x40) {
        return;
    }
    if (!EspEstSetSelect(0, 3, 0, &esp, 0)) {
        return;
    }
    e = (cEsp19*) esp;
    w = &e->w;
    e->pos = from;
    w->target = to;
    w->len *= width;
    if (pGS->flags_5010 & 1) {
        cEsp* e1 = esp;
        e1->xA4 = 1;
        asm("" : "=m"(esp) : "r"(e1), "r"(e1)); // COMPILER-DIFF: candidate (local-alloc qty order)
        esp->xA5 = 4;
        esp->xA6 = 5;
        esp->xA7 = 0;
        esp->colA *= 0.8f;
    }
}

void EspDrawLaserLine2(Vec* from, Vec* to, u8 r, u8 g, u8 b, u8 a)
{
    cEsp* esp;

    if (EspEstSetSelect(0, 3, 0, &esp, 1)) {
        cEsp19* e = (cEsp19*) esp;
        e->pos = *from;
        e->w.target = *to;
        esp->colR = (f32) r;
        esp->colG = (f32) g;
        esp->colB = (f32) b;
        esp->colA = (f32) a;
    }
    if (EspEstSetSelect(0, 3, 0, &esp, 1)) {
        cEsp19* e = (cEsp19*) esp;
        e->pos = *to;
        e->w.target = *from;
        esp->colR = (f32) r;
        esp->colG = (f32) g;
        esp->colB = (f32) b;
        esp->colA = (f32) a;
    }
}

void EspSetGatling(Vec pos, Vec dir)
{
    cEsp* esp;

    if (!EspEstSetSelect(0, 0x52, 0, &esp, 0)) {
        return;
    }
    esp->pos = pos;
    esp->spd = dir;
#line 689 "D:/Bio4/Prog/esp_app.cpp"
    VECNormalize(&esp->spd, &esp->spd);
    PSVECScale(&esp->spd, &esp->spd, 3000.0f);
    if (pG->flags_5014 & 0x02000000) {
        esp->info.x0 |= 1;
    }
}

void setPlWaterOtType()
{
    Vec* wp = &pPL->getPartsPtr(3)->worldPos;
    f32 h;

    if (GetWaterHeight(&pPL->pos, &h) && wp->y < h) {
        pPL->x12F = 0;
    } else {
        pPL->x12F = 7;
    }
}

// The original's .rodata is 8-aligned (0x2A0, 4 bytes of end padding after the last pool).
asm(".section .rodata; .balign 8");
