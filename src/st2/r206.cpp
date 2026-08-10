#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "pad.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj00.h"
#include "em.h"
#include "emBarred.h"
#include "emhit.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "math_sub.h"
#include "read.h"
#include "stage.h"
#include "sscrn.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "fade.h"
#include "mes.h"
#include "item.h"
#include "rnd.h"
#include "TexRender.h"
#include "db_log.h"

// Room 2-06 (D:/Bio4/Prog/r206.cpp): the sniper sequence where Ashley runs through the hall while
// the player covers her, Luis' body, the key item pickup and the shelf.

struct R206Work {
    cEmWrap em[14];        // 0x000  [0..2] first wave (0x60..0x62), [4..7] the four chasers (0x64..0x67, [7] = the
                           //        leader), [8..13] the three extra pairs (0x5A..0x5F)
    cObj* head;            // 0x0A8  the leader's head object
    u8 esp;                // 0x0AC  effect kind of the head fire
    u8 pad_AD[3];
    u32 snd;               // 0x0B0  SndCall handle of the taunt
    int timer;             // 0x0B4  frames until the next taunt
    int frame;             // 0x0B8  frames since Ashley's motion started
    int maxFrame;          // 0x0BC  its length
    u32 cnt3;              // 0x0C0  funcAshley3 wait counter
    u32 cnt;               // 0x0C4  taunts since the last long one
    u32 gotoNo;            // 0x0C8  leader waypoint
    TexRenderMng* tex;     // 0x0CC
    u8 texTbl[0x454 - 0xD0];  // 0x0D0
};

// The work pointer is a struct member: every store through the work reloads it.
struct R206WorkPtr {
    R206Work* p;
};

static R206WorkPtr r206_work;

// The original reads r4 although its prototype has one parameter (r20b).
void TexRenderModResP(cModel* m, int parts) asm("TexRenderModRes");
// game/shape.cpp (C++ linkage, declared locally by its users).
void ShapeSet(void* info, int a, void* data, int b);

// `pSUB->atari.flags &= 0xFCFF` through a pointer to the collision info (r207).
// Routine bytes through int parameters: one SI zero pseudo, the stores issued ff, fc, fd, fe (r210).
static inline void AtariFlagsAnd(cAtariInfo* a, u16 mask) { a->flags &= mask; }
static inline void AtariFlagsOr(cAtariInfo* a, u16 bit) { a->flags |= bit; }
// Upper half of the damage flags set = the enemy is dead (db_cam idiom); the inline's result is
// materialised as 0/1 before the test.
static inline int isDeadEm(cEm* em) { return (em->flags_324 & 0xFFFF0000) != 0; }
static inline void EmRoutineSet(cEm* p, int fc, int fd, int fe, int ff)
{
    p->xFC = fc;
    p->xFD = fd;
    p->xFE = fe;
    p->xFF = ff;
}

void r206_die_event();
static void r206_gouryuu_event();
static void item_chk();
void r206_openTerm();
SceAtWork* GetKeyItemAtari();
static void r206_auto_door_ck();
static void Evt_R206S00_Func(Event* e);
static void Evt_R206S10_Func(Event* e);
static void Evt_R206S20_Func(Event* e);
static void funcAshley(cEm* p);
static void funcAshley2(cEm* p);
static void funcAshley3(cEm* p);
int chkAliveGanadeNum();
static void r206_checkEmDead();
int fire_die_ck();
static void r206_snipe();
static void chkReaderMove();
static void r206_checkDoor();
static void r206_checkDoorToR20c();
static void r206_asl_call();
static void r206_checkDoor2();
static void r206_snipe_end();
void luis_set();
void r206_openShelf_main(int no, int opened);
static void r206_openShelf(int no);
static void r206_openedShelf(int no);
static void destroy_key_atari();

static Vec r206_ashleyPos = {5529.0f, 0.0f, -19612.0f};
Vec r206_readerPos[5] = {
    {9498.0f, 0.0f, -11899.0f},
    {-7546.0f, 0.0f, -6617.0f},
    {10381.0f, 0.0f, -2735.0f},
    {-8495.0f, 0.0f, -17240.0f},
    {8995.0f, 0.0f, -17216.0f},
};
Vec r206_chaserPos[2] = {
    {7570.0f, 0.0f, -12074.0f},
    {8693.0f, 0.0f, -9459.0f},
};
static Vec r206_hitPos0 = {0.0f, 1266.0f, 961.0f};
static Vec r206_hitPos1 = {0.0f, 937.0f, 940.0f};
static Vec r206_hitPos2 = {0.0f, 608.0f, 913.0f};
static Vec r206_hitRot = {0.0f, 0.0f, 0.0f};
static f32 r206_cubeW = 370.0f;
static f32 r206_cubeH = 120.0f;
static f32 r206_cubeD = 170.0f;
static f32 r206_cubeY = -40.0f;
static f32 r206_cubeZ = -30.0f;
static f32 r206_subCubeW = 130.0f;
static f32 r206_subCubeH = 1400.0f;
static f32 r206_subCubeD = 70.0f;
static f32 r206_subCubeY = 0.0f;
static f32 r206_subCubeZ = 0.0f;
static f32 r206_subCube2W = 60.0f;
static f32 r206_subCube2H = 1600.0f;
static f32 r206_subCube2D = 70.0f;
static f32 r206_subCube2Y = 0.0f;
static f32 r206_subCube2Z = 0.0f;
static Vec r206_ashleyGoal = {5250.0f, 0.0f, -19342.0f};

void R206Init()
{
#line 102 "D:/Bio4/Prog/r206.cpp"
    r206_work.p = (R206Work*) MEM_CALLOC(sizeof(R206Work), 1, 0xd);
    SceSetItemEvent(9, 0x80, 8, 0xA, r206_openShelf, (void (*)()) r206_openedShelf, 0, 0);
    SceAtSetEnable(0xB, 0);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        Vec pos = {0.0f, 0.0f, 900.0f};

        SubCharInit(1, &pos, -3.14f);
        SceExec(0x12, (TaskFunc) r206_snipe, 0, 0, 2, 0);
        SceExec(0x12, (TaskFunc) r206_auto_door_ck, 0, 0, 2, 0);
    } else {
        luis_set();
    }
    if (RsfCheck(G_ROOM_ID, 4) && RsfCheck(G_ROOM_ID, 9) == 0) {
        FadeSetW(1, 0, 0, 0);
        SceExec(0x12, (TaskFunc) item_chk, 0, 0, 2, 0);
    }
    if (pG->room_id_prev == 0x20D && RsfCheck(G_ROOM_ID, 4) == 0) {
        RsfSet(G_ROOM_ID, 4);
        SceExec(0x12, (TaskFunc) r206_gouryuu_event, 0, 0, 2, 0);
        BitOn(pG->flags_51C0, 0x10000000);
        BitOn(pG->flags_5018, 0x04000000);
    }
    EvtMgr.SetFunc("evt_r206s00_func", (void*) Evt_R206S00_Func);
    EvtMgr.SetFunc("evt_r206s10_func", (void*) Evt_R206S10_Func);
    EvtMgr.SetFunc("evt_r206s20_func", (void*) Evt_R206S20_Func);
    SmdSetTrans(8, 0);
    SceAtDataSet_exec(7, 0x12, 0, (TaskFunc) r206_checkDoor, 0, 1);
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        SceAtDataSet_exec(7, 0x12, 0, (TaskFunc) r206_checkDoorToR20c, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 4)) {
        SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) r206_checkDoor2, 0, 1);
        SmdSetTrans(0xC, 0);
        SmdSetTrans(0xD, 0);
        SmdSetTrans(0xE, 0);
        SceExec(0x12, (TaskFunc) destroy_key_atari, 0, 0, 2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        SceAtDataSet_exec(8, 0x12, 0, (TaskFunc) r206_asl_call, 0, 1);
    }
    SceExec(0x12, (TaskFunc) r206_snipe_end, 0, 0, 2, 0);
    TexRenderInit(&r206_work.p->tex, 0, 2);
}

void R206Main()
{
}

void r206_die_event()
{
    EvtMgr.EvtReadExec("event/evd/r206s00.evd", 0x11, 0);
    EvtMgr.EvtReadAram("event/evd/r206s10.evd", 0, 0, 0, 0);
    SceSetChapterEnd(8, -1);
}

static void r206_gouryuu_event()
{
    BitOn(pG->door_flags_51C8, 1);
    BitOn(pG->door_flags_51CC, 0x80000000);
    SceSleep(1);
    EvtMgr.EvtReadExec("event/evd/r206s20.evd", 0x11, 0);
    SceSetChapterEnd(9, -1);
    FadeSetW(1, 0, 0, 0);
    item_chk();
}

static void item_chk()
{
    u32 zero = 0;

    RsfSet(G_ROOM_ID, 9);
    ItemMgr.takeOver();
    U32Set(pG->x4F98, pG->x4F98 + pG->x832C);
    pG->x832C = zero;
    SubScreenOpen(1, 4);
    SceSleep(1);
    r206_openTerm();
}

void r206_openTerm()
{
    RsfSet(G_ROOM_ID, 7);
    OpeSetOpenTerm(0x10, 0.0f, 0.0f, 0.0f, 0.0f);
}

// The area of the key item (type 3, item 0xA3).
SceAtWork* GetKeyItemAtari()
{
    SceAtWork* p;

    p = sceAtSetOtStart();
    while ((p = sceAtGetOtAddr(p)) != NULL) {
        if (p->x35 == 3 && p->item.id == 0xA3) {
            return p;
        }
    }
    return NULL;
}

static void r206_auto_door_ck()
{
    cEm* gate0;
    cEm* gate1;

    if (getRoomEtcBarred(0x10, &gate0, 1) != 1) {
        return;
    }
    if (getRoomEtcBarred(0x11, &gate1, 1) != 1) {
        return;
    }
    ((cEmBarred*) gate0)->setClosed();
    ((cEmBarred*) gate1)->setClosed();
    for (;;) {
        if (pG->sceat_x17C & 0x80000000) {
            if (((cEmBarred*) gate0)->ckStatus() == 2) {
                ((cEmBarred*) gate0)->setOpen(0);
            }
        } else {
            if (((cEmBarred*) gate0)->ckStatus() == 1) {
                ((cEmBarred*) gate0)->setClose(0);
            }
        }
        if (pG->sceat_x17C & 0x40000000) {
            if (((cEmBarred*) gate1)->ckStatus() == 2) {
                ((cEmBarred*) gate1)->setOpen(0);
            }
        } else {
            if (((cEmBarred*) gate1)->ckStatus() == 1) {
                ((cEmBarred*) gate1)->setClose(0);
            }
        }
        SceSleep(1);
    }
}

static void Evt_R206S00_Func(Event* e)
{
    void* mod;
    void* mod2;
    void* bin;
    void* bin2;

    switch (e->funcMode) {
    case 0:
        setRoomEtcDisp(0x11, 0, 1);
        break;
    case 1:
        switch (e->cut) {
        case 0x13:
        case 0x15:
        case 0x17:
        case 0x18:
        case 0x1A:
        case 0x1E:
            if (e->frame == 0) {
                SmdSetTrans(8, 1);
            }
            break;
        default:
            if (e->frame == 0) {
                SmdSetTrans(8, 0);
            }
            break;
        }
        switch (e->cut) {
        case 0xF:
        case 0x11:
            if (e->GetMod(&mod, "obm5500", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 1, 0);
            }
            break;
        default:
            if (e->GetMod(&mod, "obm5500", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 1, 1);
            }
            break;
        }
        if (e->cut <= 0xC) {
            if (e->frame == 0) {
                if (e->GetMod(&mod2, "ev0401", 0, 0) == 1) {
                    if (EvtMgr.GetBin(&bin, "event/model/ev0400/ev0401.tpl", 0) == 1) {
                        ((cModelInfo*) mod2)->setTplAddr(bin);
                    }
                }
            }
        } else {
            if (e->frame == 0) {
                if (e->GetMod(&mod2, "ev0401", 0, 0) == 1) {
                    if (EvtMgr.GetBin(&bin2, "event/model/ev0400/ev0401_blood.tpl", 0) == 1) {
                        ((cModelInfo*) mod2)->setTplAddr(bin2);
                    }
                }
            }
        }
        switch (e->cut) {
        case 5:
        case 7:
        case 8:
        case 0xC:
        case 0xE:
        case 0xF:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "evmc800", 0, 0) == 1) {
                    ((cModel*) mod)->x12F = 1;
                    TexRenderModSet((cModel*) mod, 0, r206_work.p->texTbl, r206_work.p->tex, 0, 1, 1, 1, 1.0f);
                }
                EffectEspDelete(r206_work.p->tex->mask | 0x3001, 2, 0, 0);
                EffectEspgenDelete(r206_work.p->tex->mask | 0x3001, 2, 0);
                EffectEfmDelete(r206_work.p->tex->mask | 0x3001, 2, 0);
                EstSet(0, -1, 0, 0, 1, 2, r206_work.p->tex->mask | 0x3001, 2, 0, 0);
            }
            break;
        default:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "evmc800", 0, 0) == 1) {
                    TexRenderModResP((cModel*) mod, 0);
                }
                EffectEspDelete(r206_work.p->tex->mask | 0x3001, 2, 0, 0);
                EffectEspgenDelete(r206_work.p->tex->mask | 0x3001, 2, 0);
                EffectEfmDelete(r206_work.p->tex->mask | 0x3001, 2, 0);
            }
            break;
        }
        break;
    case 2:
        SmdSetTrans(8, 0);
        setRoomEtcDisp(0x11, 1, 1);
        break;
    }
}

static void Evt_R206S10_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        SmdSetTrans(0xC, 0);
        SmdSetTrans(0xD, 0);
        SmdSetTrans(0xE, 0);
        setRoomEtcDisp(0x11, 0, 1);
        break;
    case 1:
        break;
    case 2:
        setRoomEtcDisp(0x11, 1, 1);
        break;
    }
}

static void Evt_R206S20_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        break;
    case 1:
        if (e->cut == 0 && e->frame == 0) {
            SmdSetTrans(0x12, 0);
        }
        break;
    case 2:
        SmdSetTrans(0x12, 1);
        break;
    }
}

// Ashley's aux routine: slide towards the fixed spot while the three motions play.
static void funcAshley(cEm* p)
{
    Vec d;

    PSVECSubtract(&r206_ashleyPos, &pSUB->pos, &d);
    PSVECScale(&d, &d, 0.15f);
    PSVECAdd(&pSUB->pos, &d, &d);
    pSUB->setPos(&d);
    switch (p->xFE) {
    case 0:
        AtariFlagsAnd(&pSUB->atari, 0xFCFF);
        p->motionSet(ROOM_ARC_PTR(pGS->pRoomArc, 0x21), 0xA, 0, 1, 0);
        SndCall(6, 0xD, &pSUB->pos, 0, 0, 0);
        p->xFE = 1;
    case 1:
        if (p->motionMove() != 0) {
            p->xFE = 2;
        }
        break;
    case 2:
        p->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x22), 0xA, 0, 1, 0);
        p->xFE = 3;
    case 3:
        if (p->motionMove() != 0) {
            p->xFE = 4;
        }
        break;
    case 4:
        p->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x23), 0xA, 0, 1, 0);
        p->xFE = 5;
    default:
        if (p->motionMove() != 0) {
            EmRoutineSet(p, 0, 0, 0, 0);
            AtariFlagsOr(&pSUB->atari, 0x300);
            SubCharCtrl(1, 0);
        }
        break;
    }
    p->rot.y = 2.35f;
}

static void funcAshley2(cEm* p)
{
    if (p->xFE == 0) {
        AtariFlagsAnd(&pSUB->atari, 0xFCFF);
        p->motionSet(ROOM_ARC_PTR(pGS->pRoomArc, 0x27), 0x19, 0, 1, 0);
        p->xFE = 1;
    }
    if (p->motionMove() != 0) {
        EmRoutineSet(p, 0, 0, 0, 0);
        AtariFlagsOr(&pSUB->atari, 0x300);
        SubCharCtrl(1, 0);
    }
}

static void funcAshley3(cEm* p)
{
    int step = p->xFE;

    switch (step) {
    case 0:
        AtariFlagsAnd(&pSUB->atari, 0xFCFF);
        p->motionSet(ROOM_ARC_PTR(pGS->pRoomArc, 0x31), 0x19, 0, 1, 0);
        r206_work.p->cnt3 = step;
        p->xFE = 1;
    case 1:
        if (p->motionMove() != 0) {
            p->xFE = 2;
        }
        break;
    case 2:
        r206_work.p->cnt3++;
        if (r206_work.p->cnt3 > 0x1E) {
            p->xFE = 3;
        }
        break;
    default:
        EmRoutineSet(p, 0, 0, 0, 0);
        AtariFlagsOr(&pSUB->atari, 0x300);
        SubCharCtrl(1, 0);
        break;
    }
}

int chkAliveGanadeNum()
{
    int cnt = 0;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if (em->id >= 0x10 && em->id <= 0x20 && em->checkStatus(5) != 0 && (em->be_flag & 0x201) == 1) {
            cnt++;
        }
    }
    return cnt;
}

static void r206_checkEmDead()
{
    while (SceAtItemFlgCk(0x82) == 0) {
        SceSleep(1);
    }
    r206_work.p->head->be_flag &= ~2;
    EffectEspDelete(0, r206_work.p->esp, 0, 0);
    EffectEspgenDelete(0, r206_work.p->esp, 0);
    EffectEfmDelete(0, r206_work.p->esp, 0);
}

int fire_die_ck()
{
    Vec out;
    int kind;

    kind = DmgMgr.hitCheck(&pSUB->pos, &out);
    if (kind != 1 && kind != 7) {
        return 0;
    }
    return 1;
}

static void r206_snipe()
{
    cObj* obj0;
    cObj* obj1;
    cObj* obj2;
    cEmHit* hit0 = NULL;
    cEmHit* hit1 = NULL;
    cEmHit* hit2 = NULL;
    cEmHit* subHit0;
    cEmHit* subHit1;
    cEm* gate;
    SceAtWork* at;
    int done;
    int moved;
    int wave;

    obj0 = SmdGetObjPtr(0xC);
    obj1 = SmdGetObjPtr(0xD);
    obj2 = SmdGetObjPtr(0xE);
    BitOn(pG->flags_51BC, 0x80);
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        hit0 = SetEmHit(ROOM_ARC_PTR(pG->pArc, 8), ROOM_ARC_PTR(pG->pArc, 9), &r206_hitPos0, &r206_hitRot, 0);
        BitOn(obj0->be_flag, 0x20);
        YarareInitCube(hit0, 0.0f, r206_cubeY, r206_cubeZ, r206_cubeW, r206_cubeH, r206_cubeD, 0, 1);
    } else {
        obj0->be_flag &= ~2;
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        hit1 = SetEmHit(ROOM_ARC_PTR(pG->pArc, 8), ROOM_ARC_PTR(pG->pArc, 9), &r206_hitPos1, &r206_hitRot, 0);
        BitOn(obj1->be_flag, 0x20);
        YarareInitCube(hit1, 0.0f, r206_cubeY, r206_cubeZ, r206_cubeW, r206_cubeH, r206_cubeD, 0, 1);
    } else {
        obj1->be_flag &= ~2;
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        hit2 = SetEmHit(ROOM_ARC_PTR(pG->pArc, 8), ROOM_ARC_PTR(pG->pArc, 9), &r206_hitPos2, &r206_hitRot, 0);
        BitOn(obj2->be_flag, 0x20);
        YarareInitCube(hit2, 0.0f, r206_cubeY, r206_cubeZ, r206_cubeW, r206_cubeH, r206_cubeD, 0, 1);
    } else {
        obj2->be_flag &= ~2;
    }
    subHit0 = SetEmHit(ROOM_ARC_PTR(pG->pArc, 8), ROOM_ARC_PTR(pG->pArc, 9), &pSUB->pos, &r206_hitRot, 1);
    YarareInitCube(subHit0, 0.0f, r206_subCubeY, r206_subCubeZ, r206_subCubeW, r206_subCubeH, r206_subCubeD, 0, 1);
    subHit1 = SetEmHit(ROOM_ARC_PTR(pG->pArc, 8), ROOM_ARC_PTR(pG->pArc, 9), &pSUB->pos, &r206_hitRot, 1);
    YarareInitCube(subHit1, 0.0f, r206_subCube2Y, r206_subCube2Z, r206_subCube2W, r206_subCube2H, r206_subCube2D, 0, 1);
    if (DebugTrg(1) == 0) {
        SceSleep(1);
        if (RsfCheck(G_ROOM_ID, 5) == 0) {
            RsfSet(G_ROOM_ID, 5);
            r206_die_event();
        }
        luis_set();
    }
    SubCharCtrl(5, 0);
    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x20), 0, 0, 5, 0);
    pSUB->st.x325 = 0x80;
    {
        Vec pos = {0.0f, 0.0f, 900.0f};

        pSUB->setPos(&pos);
    }
    r206_work.p->timer = 0xF0;
    do {
        r206_work.p->timer--;
        if (r206_work.p->timer == 0) {
            u32 rsf;
            u32 zero;

            r206_work.p->timer = (u32) (fRand0_1() * 60.0f) + 0x3C;
            rsf = RsfCheck(*(u16*) &pGS->stage_no, 6);
            zero = 0;
            if (rsf) {
                r206_work.p->cnt++;
            }
            if (r206_work.p->cnt > 8) {
                U32Set(r206_work.p->cnt, zero);
                r206_work.p->snd = SndCall(6, 5, &pSUB->pos, 0, 0, 0);
            } else {
                r206_work.p->snd = SndCall(6, 6, &pSUB->pos, 0, 0, 0);
            }
        }
        done = 0;
        if (RsfCheck(G_ROOM_ID, 1) == 0) {
            if (hit0->ckStatus() == 1) {
                RsfSet(G_ROOM_ID, 1);
                EstSet(0, -1, &r206_hitPos0, &r206_hitRot, 1, 0, 0, 0, 0, 0);
                obj0->be_flag &= ~2;
                if (r206_work.p->snd != 0) {
                    SndStop(r206_work.p->snd, 0);
                }
                SndCall(6, 4, &obj0->pos, 0, 0, 0);
                SndCall(6, 0xA, &obj0->pos, 0, 0, 0);
                r206_work.p->timer = 0x5A;
            }
        } else {
            done = 1;
        }
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            if (hit1->ckStatus() == 1) {
                RsfSet(G_ROOM_ID, 2);
                EstSet(0, -1, &r206_hitPos1, &r206_hitRot, 1, 0, 0, 0, 0, 0);
                obj1->be_flag &= ~2;
                if (r206_work.p->snd != 0) {
                    SndStop(r206_work.p->snd, 0);
                }
                SndCall(6, 4, &obj1->pos, 0, 0, 0);
                if (done == 0) {
                    SndCall(6, 0xA, &obj1->pos, 0, 0, 0);
                }
                r206_work.p->timer = 0x5A;
            }
        } else {
            done++;
        }
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            if (hit2->ckStatus() == 1) {
                RsfSet(G_ROOM_ID, 3);
                EstSet(0, -1, &r206_hitPos2, &r206_hitRot, 1, 0, 0, 0, 0, 0);
                obj2->be_flag &= ~2;
                if (r206_work.p->snd != 0) {
                    SndStop(r206_work.p->snd, 0);
                }
                SndCall(6, 4, &obj2->pos, 0, 0, 0);
                if (done == 0) {
                    SndCall(6, 0xA, &obj2->pos, 0, 0, 0);
                }
                r206_work.p->timer = 0x5A;
            }
        } else {
            done++;
        }
        if (RsfCheck(G_ROOM_ID, 1) && RsfCheck(G_ROOM_ID, 2)) {
            if (r206_work.p->frame == 0) {
                pSUB->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x25), 0, 0, 1, 0);
                *(u32*) &r206_work.p->maxFrame = (u32) MotionGetMaxFrame(&pSUB->mot);
                YarareInitCube(subHit0, 0.0f, r206_subCubeY, r206_subCubeZ, r206_subCubeW, r206_subCubeH * 0.85f,
                               r206_subCubeD, 0, 1);
                YarareInitCube(subHit1, 0.0f, r206_subCube2Y, r206_subCube2Z, r206_subCube2W, r206_subCube2H * 0.7f,
                               r206_subCube2D, 0, 1);
            } else if (r206_work.p->frame == r206_work.p->maxFrame) {
                pSUB->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x26), 0, 0, 5, 0);
            }
            r206_work.p->frame++;
        }
        if ((pG->flags_68 & 0x00800000) == 0) {
            if (fire_die_ck() == 1 || subHit0->ckStatus() == 1 || subHit1->ckStatus() == 1) {
                *(s16*) &pG->sub_life = -1;
                if (RsfCheck(G_ROOM_ID, 1) && RsfCheck(G_ROOM_ID, 2)) {
                    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x2E), 0, 0, 1, 0);
                } else {
                    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x24), 0, 0, 1, 0);
                }
            }
        }
        if ((s16) pG->sub_life != -1 && (u32) done > 2) {
            EmRoutineSet(pSUB, 0, 0, 0, 0);
            goto snipe_done;
        }
        SceSleep(1);
    } while (1);
snipe_done:
    SceSleep(1);
    EvtMgr.EvtReadExec("event/evd/r206s10.evd", 0x11, 0);
    FadeSetW(0x80000002, 0x1E, 0, 0);
    {
        Vec pos = {0.0f, 0.0f, -1740.0f};

        pSUB->setPos(&pos);
    }
    SceAtSetEnable(8, 0);
    RsfSet(G_ROOM_ID, 6);
    pSUB->dmg.clear();
    SubCharMoveTo(1, 0.0f, 0.0f, -2000.0f, 193.0f);
    r206_work.p->em[0].setEm(0x60, -1, 1, 1, 1);
    r206_work.p->em[1].setEm(0x61, -1, 1, 1, 1);
    r206_work.p->em[2].setEm(0x62, -1, 1, 1, 1);
    r206_work.p->em[0].setBeFlag(0x10000, 1);
    r206_work.p->em[1].setBeFlag(0x10000, 1);
    r206_work.p->em[2].setBeFlag(0x10000, 1);
    SceAtDataSet_exec(5, 0x12, 0, (TaskFunc) r206_checkDoorToR20c, 0, 1);
    SceSleep(1);
    while (chkAliveGanadeNum() != 0) {
        SceSleep(1);
    }
    if ((pG->flags_174 & 0x80000000) == 0) {
        pG->flags_174 |= 0x80000000;
    }
    SetSubAux((int) funcAshley2, 0);
    SceSleep(1);
    while (SubCharGetStatus() & 0x01000000) {
        SceSleep(1);
    }
    SubCharMoveTo(1, 4316.0f, 0.0f, -6652.0f, 193.0f);
    while ((SubCharGetStatus() & 0x00800000) == 0) {
        SceSleep(1);
    }
    SubCharMoveTo(0, 0.0f, 0.0f, -2300.0f, 193.0f);
    SubCharMoveTo(0, r206_ashleyGoal.x, r206_ashleyGoal.y, r206_ashleyGoal.z, 193.0f);
    SceSleep(1);
    while ((SubCharGetStatus() & 0x00800000) == 0) {
        SceSleep(1);
    }
    SetSubAux((int) funcAshley, 0);
    pSUB->setNoSuspend(1);
    SceEventStart(1);
    pG->flags_170 &= ~0x20000000;
    CamCtrl.CutCall(3);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSleep(0x32);
    SndCall(6, 0xB, 0, 0, 0, 0);
    SceMesSet(2, 0xA2, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
    SceSleep(0xA);
    SceSleep(0x37);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    pSUB->setNoSuspend(0);
    r206_work.p->gotoNo = 0;
    r206_work.p->em[4].setEm(0x64, -1, 1, 1, 1);
    r206_work.p->em[5].setEm(0x65, -1, 1, 1, 1);
    r206_work.p->em[6].setEm(0x66, -1, 1, 1, 1);
    r206_work.p->em[7].setEm(0x67, -1, 1, 1, 1);
    r206_work.p->em[4].setBeFlag(0x10000, 1);
    r206_work.p->em[5].setBeFlag(0x10000, 1);
    r206_work.p->em[6].setBeFlag(0x10000, 1);
    if (r206_work.p->em[7].isAlive() == 1) {
        Vec ofs = {0.0f, -2.0f, 180.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};

        r206_work.p->head = SetObj00(ROOM_ARC_PTR(pG->pRoomArc, 0x2F), ROOM_ARC_PTR(pG->pRoomArc, 0x30), &ofs, &rot);
        r206_work.p->head->setNoSuspend(1);
        OyaSetObj00(r206_work.p->head, r206_work.p->em[7].getPtr(), 2);
        r206_work.p->esp = EspPullCoreKind();
        SceExec(0x12, (TaskFunc) r206_checkEmDead, 0, 0, 2, 0);
    }
    r206_work.p->em[4].setNoSuspend(1);
    r206_work.p->em[5].setNoSuspend(1);
    r206_work.p->em[7].setNoSuspend(1);
    if (getRoomEtcBarred(0x11, &gate, 1) == 1) {
        gate->setNoSuspend(1);
        ((cEmBarred*) gate)->setOpen(0);
    }
    SceEventStart(1);
    CamCtrl.CutCall(4);
    r206_work.p->em[7].setGoto(&r206_readerPos[r206_work.p->gotoNo], 1);
    while (r206_work.p->em[7].ckGoto() != 0) {
        SceSleep(1);
    }
    r206_work.p->em[7].setGoto(&r206_readerPos[r206_work.p->gotoNo], 8);
    r206_work.p->em[4].setGoto(&r206_readerPos[r206_work.p->gotoNo], 1);
    r206_work.p->em[5].setGoto(&r206_chaserPos[0], 1);
    r206_work.p->em[6].setGoto(&r206_chaserPos[0], 1);
    r206_work.p->em[5].setFindPL();
    r206_work.p->em[6].setFindPL();
    SceSleep(0x37);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r206_work.p->em[4].setNoSuspend(0);
    r206_work.p->em[5].setNoSuspend(0);
    r206_work.p->em[6].setNoSuspend(0);
    r206_work.p->em[7].setNoSuspend(0);
    SceExec(0x12, (TaskFunc) chkReaderMove, 0, 0, 2, 0);
    SubCharMoveTo(0, 4316.0f, 0.0f, -6652.0f, 193.0f);
    moved = 0;
    wave = 1;
    for (;;) {
        if (moved == 0 && (SubCharGetStatus() & 0x00800000) == 0) {
            moved = 1;
            SubCharMoveTo(0, 0.0f, 0.0f, -2300.0f, 193.0f);
        }
        if (r206_work.p->em[7].isActive() == 0) {
            goto wave_done;
        }
        if ((u32) SceCountEmAlive(0x10, 0x20) <= 2) {
            r206_work.p->em[7].setGoto(&pSUB->pos, 8);
            if (wave == 0) {
                r206_work.p->em[8].setEm(0x5A, -1, 1, 1, 1);
                r206_work.p->em[9].setEm(0x5B, -1, 1, 1, 1);
                r206_work.p->em[8].setFindPL();
                r206_work.p->em[9].setFindPL();
                r206_work.p->em[8].setBeFlag(0x10000, 1);
                r206_work.p->em[9].setBeFlag(0x10000, 1);
            } else if (wave == 1) {
                r206_work.p->em[10].setEm(0x5C, -1, 1, 1, 1);
                r206_work.p->em[11].setEm(0x5D, -1, 1, 1, 1);
                r206_work.p->em[10].setFindPL();
                r206_work.p->em[11].setFindPL();
                r206_work.p->em[10].setBeFlag(0x10000, 1);
                r206_work.p->em[11].setBeFlag(0x10000, 1);
            } else if (wave == 2) {
                r206_work.p->em[12].setEm(0x5E, -1, 1, 1, 1);
                r206_work.p->em[13].setEm(0x5F, -1, 1, 1, 1);
                r206_work.p->em[12].setFindPL();
                r206_work.p->em[13].setFindPL();
                r206_work.p->em[12].setBeFlag(0x10000, 1);
                r206_work.p->em[13].setBeFlag(0x10000, 1);
                goto wave_done;
            }
            wave++;
        }
        SceSleep(1);
    }
wave_done:
    for (;;) {
        if (moved == 0 && (SubCharGetStatus() & 0x00800000) == 0) {
            moved = 1;
            SubCharMoveTo(0, 0.0f, 0.0f, -2300.0f, 193.0f);
        }
        if (SceCountEmAlive(0x10, 0x20) == 0) {
            break;
        }
        SceSleep(1);
    }
    at = GetKeyItemAtari();
    if (at != NULL) {
        SubCharMoveTo(0, at->item.pos.x, at->item.pos.y, at->item.pos.z, 193.0f);
    } else {
        pLog->err(0, 0, "KEY ATARI NOT FOUND!!");
    }
    while ((SubCharGetStatus() & 0x00800000) == 0) {
        SceSleep(1);
    }
    SetSubAux((int) funcAshley3, 0);
    SceSleep(0xA);
    if (at != NULL) {
        SceAtSetEnable(at->no, 0);
    }
    SceSleep(0xF);
    SndCall(6, 0xC, 0, 0, 0, 0);
    SceMesSet(4, 0xA2, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
    while (SubCharGetStatus() & 0x01000000) {
        SceSleep(1);
    }
    SubCharMoveTo(0, 5316.0f, 0.0f, -18975.0f, 2.27f);
    while ((SubCharGetStatus() & 0x00800000) == 0) {
        SceSleep(1);
    }
    pSUB->setNoSuspend(1);
    SceEventStart(1);
    {
        Vec pos;
        cModel* m = pSUB;

        pos.x = 5669.0f;
        pos.y = 0.0f;
        pos.z = -19307.0f;
        m->setPos(&pos);
    }
    CamCtrl.CutCall(3);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSleep(1);
    SndCall(6, 3, 0, 0, 0, 0);
    SceMesSet(5, 2, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
    if (pSUB != NULL) {
        EmMgr.destroy(pSUB);
        pG->flags_5018 &= ~0x04000000;
    }
    pG->flags_51C4 |= 0x08000000;
    PlSelect(1);
    SceAtExecute(0);
    RsfSet(G_ROOM_ID, 0);
    SceEventEnd(0);
}

// The leader walks the waypoints; the chasers follow Ashley once few enemies remain.
static void chkReaderMove()
{
    int timer = 0;
    u32 cnt = 0;
    int near = 0;
    cEm* em;

    while (r206_work.p->em[7].isActive() != 0) {
        cnt++;
        em = r206_work.p->em[7].getPtr();
        if (em != NULL) {
            Vec d;
            f32 dist;
            int n;

            PSVECSubtract(&em->pos, &r206_readerPos[r206_work.p->gotoNo], &d);
            dist = PSVECMag(&d);
            if (dist < 2500.0f) {
                if (dist < 1000.0f && near == 0) {
                    near = 1;
                    r206_work.p->em[7].setGoto(&pSUB->pos, 8);
                }
                if ((r206_work.p->gotoNo == 0 && cnt == 0x1A4)
                    || (cnt > 0x257 && (em->be_flag & 0x201) == 1 && em->checkStatus(5) == 1
                        && isDeadEm(em))) {
                    timer = 0x5A;
                }
                if (timer != 0) {
                    timer--;
                }
                if (timer == 1) {
                    r206_work.p->gotoNo++;
                    if (r206_work.p->gotoNo > 4) {
                        r206_work.p->gotoNo = 0;
                    }
                    near = 0;
                    r206_work.p->em[7].setGoto(&r206_readerPos[r206_work.p->gotoNo], 1);
                    r206_work.p->em[4].setGoto(&r206_readerPos[r206_work.p->gotoNo], 1);
                    r206_work.p->em[6].setGoto(&r206_readerPos[r206_work.p->gotoNo], 1);
                }
            } else {
                r206_work.p->em[7].setGoto(&r206_readerPos[r206_work.p->gotoNo], 1);
            }
            n = SceCountEmAlive(0x10, 0x20);
            if (n == 0) {
                break;
            }
            if (n == 1) {
                r206_work.p->em[7].setGoto(&pSUB->pos, 1);
                if (em->hp > 0x2BC) {
                    em->hp = 0x2BC;
                }
                break;
            }
        }
        SceSleep(1);
    }
    for (;;) {
        int n = SceCountEmAlive(0x10, 0x20);

        if (n == 1) {
            if (r206_work.p->em[4].isActive() != 0) {
                r206_work.p->em[4].getPtr()->flags_3C8 |= 0x40;
            }
            if (r206_work.p->em[6].isActive() != 0) {
                r206_work.p->em[6].getPtr()->flags_3C8 |= 0x40;
            }
            r206_work.p->em[4].setGoto(&pSUB->pos, 1);
            r206_work.p->em[6].setGoto(&pSUB->pos, 1);
            SceSleep(0x168);
        }
        if (n == 2) {
            if (r206_work.p->em[4].isActive() != 0 && r206_work.p->em[6].isActive() != 0) {
                if (r206_work.p->em[4].isActive() != 0) {
                    r206_work.p->em[4].getPtr()->flags_3C8 |= 0x40;
                }
                r206_work.p->em[4].setGoto(&pSUB->pos, 1);
                SceSleep(0x168);
            }
        }
        SceSleep(1);
    }
}

static void r206_checkDoor()
{
    SceAtDataReset(7);
}

static void r206_checkDoorToR20c()
{
    cMes.MesSet(0x67, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1, 1, 0, 0, 4);
}

static void r206_asl_call()
{
    r206_work.p->timer = 0x5A;
    SndStop(r206_work.p->snd, 0);
    if (pSUB != NULL) {
        pSUB->setNoSuspend(1);
    }
    SceEventStart(1);
    r206_work.p->snd = SndCall(6, 5, &pSUB->pos, 0, 0, 0);
    CamCtrl.CutCall(9);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    if (pSUB != NULL) {
        pSUB->setNoSuspend(0);
    }
    RsfSet(G_ROOM_ID, 6);
}

static void r206_checkDoor2()
{
    SceMesSet(6, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
}

static void r206_snipe_end()
{
    for (;;) {
        if (DebugTrg(0) == 0) {
            SceSleep(1);
        } else {
            break;
        }
    }
    PlSelect(1);
    SceAtExecute(0);
}

// Luis' body and the blood.
void luis_set()
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;
    f32 zero;
    int lit = 4;

    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoomArc, 0x32), ROOM_ARC_PTR(pG->pRoomArc, 0x33), &pos, &rot, 0x10, 1);
    obj->addModel(ModInfoMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x34), ROOM_ARC_PTR(pG->pRoomArc, 0x35)));
    obj->addModel(ModInfoMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x36), ROOM_ARC_PTR(pG->pRoomArc, 0x37)));
    obj->addModel(ModInfoMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x38), ROOM_ARC_PTR(pG->pRoomArc, 0x39)));
    obj->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x3D), 0xA, 0, 1, 0);
    obj->be_flag |= 0x1000;
    zero = 0.0f;
    obj->motSpeedRate = zero;
    obj->setNoSuspend(1);
    obj->lightInfo.x50 = lit;
    obj->be_flag |= 0x10;
    ShapeSet(obj->pInfo->pNext->pNext, 0, ROOM_ARC_PTR(pG->pRoomArc, 0x3C), 2);
    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoomArc, 0x3A), ROOM_ARC_PTR(pG->pRoomArc, 0x3B), &pos, &rot, 0x10, 1);
    obj->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x3C), 0xA, 0, 1, 0);
    obj->be_flag |= 0x1000;
    obj->motSpeedRate = zero;
    obj->setNoSuspend(1);
    obj->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x3E), 0xA, 0, 1, 0);
    obj->lightInfo.x50 = lit;
    obj->be_flag |= 0x10;
    EstSet((int) obj, -1, 0, 0, 1, 3, 1, 0, 0, 0);
    SceAtSetEnable(0xB, 1);
}

void r206_openShelf_main(int no, int opened)
{
    cObj* obj;

    OpenBoxMain(0x15, opened, 0x15, 9, -1, -1);
    obj = SmdGetObjPtr(9);
    if (obj != NULL) {
        Vec* rot = &obj->rot;

        obj->pos.x = -401.0f;
        obj->pos.y = 8028.0f;
        obj->pos.z = -4054.0f;
        obj->rot.x = 1.5592f;
        obj->rot.y = 1.42932f;
        obj->rot.z = 0.0f;
        obj->pParts->rot.x = 0.0f;
        obj->pParts->rot.y = 0.0f;
        obj->pParts->rot.z = 0.0f;
        obj->setPos(&obj->pos);
        obj->setAng(rot);
    }
}

static void r206_openShelf(int no)
{
    r206_openShelf_main(no, 0);
}

static void r206_openedShelf(int no)
{
    r206_openShelf_main(no, 1);
}

static void destroy_key_atari()
{
    SceAtWork* at;

    SceSleep(1);
    at = GetKeyItemAtari();
    if (at != NULL) {
        SceAtSetEnable(at->no, 0);
    } else {
        pLog->err(0, 0, "KEY ATARI NOT FOUND!!");
    }
}
