// The split .rodata is 8-aligned (0x250) while the unit has no double constant.
asm(".section .rodata\n\t.balign 8\n\t.text");
#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "emdoor.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "mes.h"
#include "motion.h"
#include "math_sub.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "TexRender.h"
#include "db_log.h"

// Room 2-05 (D:/Bio4/Prog/r205.cpp): the swinging pendulum blades over the pit, the drain event,
// the enemy waves it releases and a texture-rendered object.

// One pendulum blade: the scroll object and its rotation of the current / previous frame
// (SceAtDataSet_exec hands the entry to r205_ExecDieDemo as the task argument).
struct R205Pend {
    cObj* obj;     // 0x00
    f32 rotPrev;   // 0x04
    f32 rot;       // 0x08
};

struct R205Em {
    int dead;      // 0x00
    cEmWrap em;    // 0x04
};

struct R205Work {
    TexRenderMng* tex;   // 0x00
    u8 pad_4[4];
    R205Pend pend[4];    // 0x08
    cObj* pole[4];       // 0x38  the blade poles that rise with the swing ([1] unused)
    cEmHit* hit[4];      // 0x48
    R205Em ems[8];       // 0x58
    cEm* door0;          // 0xD8
    cEm* door1;          // 0xDC
};

static u8 r205_texTbl[0x20];
// The work pointer is a struct member: every store through the work reloads it.
struct R205WorkPtr {
    R205Work* p;
};

static R205WorkPtr r205_work;

int r205_rsfTbl[7] = {1, 2, 3, 4, 6, 7, 8};
static int r205_emNoTbl[7] = {0x68, 0x69, 0x6A, 0x6B, 0x6E, 0x6F, 0x58};
static int r205_emIdxTbl[6] = {0, 1, 2, 3, 4, 5};
static Vec r205_sePos = {11600.0f, 1000.0f, 730.0f};
static int r205_camParts = 2;

// Hit effects of attribute type 2
static const AtEffInfo r205_effInfo = {
    1, {0xD2, 0x2C}, {0xD2, 0x2F}, {0xD2, 0x2E}, {0xD2, 0x2D}, {0xD2, 0x20}, {0xD2, 0x20}, {0xD2, 0}, {0xD2, 0},
};

// The original passes an uninitialised int to cEmDoor::setCloseLock(int) (no r4 setup, r105 idiom).
void cEmDoorSetCloseLock(cEm* door) asm("setCloseLock__7cEmDoori");
// COMPILER-DIFF: #4 — the table entry is an int; the original passes it to the s16 parameter
// without a truncation.
int cEmWrapSetEmI(cEmWrap* w, int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__7cEmWrapsSciii");

// cUnit::beginEvent / endEvent take an int in the original (see sscrn.cpp).
class cUnitEvent {
public:
    u32 be_flag;
    cUnit* next;
    virtual ~cUnitEvent();
    virtual void beginEvent(int mode);
    virtual void endEvent(int mode);
};
#define BEGIN_EVENT(p, mode) ((cUnitEvent*) (p))->beginEvent(mode)

void r205_Em105AppearCheck();
void r205_Em106AppearCheck();
void r205_Em107AppearCheck();
void r205_Em110AppearCheck();
void r205_Em111AppearCheck();
static void r205_Em88Appear();
int r205_AtHitCheckEmNum(int atA, int atB);
static void r205_PendulumMove();
static void r205_ExecDieDemo(R205Pend* p);
static void r205_DrainEvent();
static void r205_DrainEventEnd();
static void r205_StrCheck();
static void setTexRender();
static void r205_EnemyAppear();
static void r205_EnemyAppearEndProc();
static void r205_RoomExitFunc();
static void r205_TreasureBoxOpen(int id);
static void r205_TreasureBoxOpened(int id);
static void r205_ContinuePointSet();

void R205Init()
{
    // The work address is taken before the calloc call (`lis` above the `bl`, as the plain-pointer
    // rooms do): the store goes through a pointer to the member.
    R205Work** wp;
    u32 i;

    pG->flags_5010 |= 0x400;
    wp = &r205_work.p;
#line 103 "D:/Bio4/Prog/r205.cpp"
    *wp = (R205Work*) MEM_CALLOC(sizeof(R205Work), 1, 0xd);
    if (RsfCheck(G_ROOM_ID, 12) == 0) {
        SceAtDataSet_exec(0x1C, 0x12, 0, (TaskFunc) r205_ContinuePointSet, 0, 1);
    }
    setTexRender();
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(8, 0x12, 0, (TaskFunc) r205_DrainEvent, 0, 1);
        SetSstDispFlag(0xC, 1);
        SetSstDispFlag(0xD, 0);
        SceAtSetEnable(0x11, 0);
        EatMgr.registEffInfo(2, (AtEffInfo*) &r205_effInfo);
    } else {
        SceAtSetEnable(8, 0);
        SceAtSetEnable(7, 0);
        SetSstDispFlag(0xC, 0);
        SetSstDispFlag(0xD, 1);
        SceAtSetEnable(0x15, 0);
    }
    if (RsfCheck(G_ROOM_ID, 9) == 0) {
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) r205_EnemyAppear, 0, 1);
    }
    r205_work.p->pend[0].obj = SmdGetObjPtr(5);
    r205_work.p->pend[1].obj = SmdGetObjPtr(6);
    r205_work.p->pend[2].obj = SmdGetObjPtr(7);
    r205_work.p->pend[3].obj = SmdGetObjPtr(8);
    r205_work.p->pole[0] = SmdGetObjPtr(0x6B);
    r205_work.p->pole[1] = NULL;
    r205_work.p->pole[2] = SmdGetObjPtr(0x6C);
    r205_work.p->pole[3] = SmdGetObjPtr(0x6D);
    SceExec(0x12, (TaskFunc) r205_PendulumMove, 0, 0, 2, 0);
    SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) r205_ExecDieDemo, &r205_work.p->pend[0], 1);
    SceAtDataSet_exec(0xA, 0x12, 0, (TaskFunc) r205_ExecDieDemo, &r205_work.p->pend[1], 1);
    SceAtDataSet_exec(0xB, 0x12, 0, (TaskFunc) r205_ExecDieDemo, &r205_work.p->pend[2], 1);
    SceAtDataSet_exec(0x12, 0x12, 0, (TaskFunc) r205_ExecDieDemo, &r205_work.p->pend[3], 1);
    if (r205_work.p->ems[6].em.setEm(0x6C, -1, 0, 1, 1) == 0) {
        r205_work.p->ems[6].dead = 1;
    }
    if (r205_work.p->ems[7].em.setEm(0x6D, -1, 0, 1, 1) == 0) {
        r205_work.p->ems[7].dead = 1;
    }
    for (i = 0; i < 6; i++) {
        if (RsfCheck(G_ROOM_ID, r205_rsfTbl[i])) {
            if (cEmWrapSetEmI(&r205_work.p->ems[r205_emIdxTbl[i]].em, r205_emNoTbl[i], -1, 0, 1, 1) == 0) {
                r205_work.p->ems[r205_emIdxTbl[i]].dead = 1;
            }
        }
    }
    SceAtSetEnable(0x21, 0);
    SceAtSetEnable(0x22, 0);
    if (getRoomEtcDoor(0x13, &r205_work.p->door0, 1) != 0) {
        if (RsfCheck(G_ROOM_ID, 7) == 0) {
            cEmDoorSetCloseLock(r205_work.p->door0);
        }
    }
    if (getRoomEtcDoor(0x14, &r205_work.p->door1, 1) != 0) {
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            cEmDoorSetCloseLock(r205_work.p->door1);
        }
    }
    SceExec(0x12, (TaskFunc) r205_StrCheck, 0, 0, 2, 0);
    {
        Vec pos = {0.0f, -425.0f, 0.0f};

        for (i = 0; i < 4; i++) {
            r205_work.p->hit[i] = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc),
                                         &pos, 0, 1);
            r205_work.p->hit[i]->setParent(r205_work.p->pend[i].obj, 0, 0);
            YarareInitCube(r205_work.p->hit[i], 0.0f, -400.0f, 0.0f, 1700.0f, 1300.0f, 100.0f, 0, 1);
        }
    }
    SceAtSetDoorFunc(0, (TaskFunc) r205_RoomExitFunc, 0);
    SceSetItemEvent(5, 0x8E, 0xB, 0xB, r205_TreasureBoxOpen, (void (*)()) r205_TreasureBoxOpened, 0x8B, 0);
}

void R205Main()
{
    u32 i;

    pPL->x12F = 5;
    for (i = 0; i < 8; i++) {
        R205Em* e = &r205_work.p->ems[i];

        if (e->dead == 0) {
            if (e->em.isAlive() == 1 && e->em.isActive() == 0) {
                e->dead = 1;
            }
        }
    }
    if (SceAtHitCheck(0xC) == 1) {
        pG->sceat_x17C |= 0x20000000;
    } else if (SceAtHitCheck(0xD) == 1) {
        pG->sceat_x17C |= 0x40000000;
    } else if (SceAtHitCheck(0xE) == 1) {
        pG->sceat_x17C |= 0x40000000;
    } else if (SceAtHitCheck(0xF) == 1 || SceAtHitCheck(0x10) == 1) {
        pG->sceat_x17C |= 0x80000000;
    }
    r205_Em105AppearCheck();
    r205_Em106AppearCheck();
    r205_Em107AppearCheck();
    r205_Em110AppearCheck();
    r205_Em111AppearCheck();
    if (SceAtHitCheck(0x17) != 0) {
        if (r205_work.p->ems[6].dead == 0) {
            r205_work.p->ems[6].em.setFlag(1);
        }
    }
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        if (SceAtHitCheck(0x16) == 1) {
            RsfSet(G_ROOM_ID, 10);
            RoomSeCall(4, &r205_sePos, 0, 0, 0);
        }
    }
}

void r205_Em105AppearCheck()
{
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceAtSetEnable(0x22, 1);
        if (r205_work.p->ems[6].dead == 1) {
            Vec pos;

            RsfSet(G_ROOM_ID, 2);
            r205_work.p->ems[1].em.setEm(0x69, -1, 1, 1, 1);
            r205_work.p->ems[1].em.setFlag(1);
            r205_work.p->ems[1].em.getPos(&pos);
            ((cEmDoor*) r205_work.p->door1)->setOpen(&pos, 0, 0, 0);
            SceAtSetEnable(0x22, 0);
        }
    }
}

void r205_Em106AppearCheck()
{
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        if ((int) pG->sceat_x17C < 0) {
            if (r205_work.p->ems[0].dead == 1 && r205_work.p->ems[7].dead == 1) {
                RsfSet(G_ROOM_ID, 3);
                r205_work.p->ems[2].em.setEm(0x6A, -1, 1, 1, 1);
            }
        }
    }
}

void r205_Em107AppearCheck()
{
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        if (pG->sceat_x17C & 0x40000000) {
            if (RsfCheck(G_ROOM_ID, 2) && RsfCheck(G_ROOM_ID, 7)) {
                if ((u32) r205_AtHitCheckEmNum(0xD, 0xE) <= 1) {
                    RsfSet(G_ROOM_ID, 4);
                    r205_work.p->ems[3].em.setEm(0x6B, -1, 1, 1, 1);
                }
            }
        }
    }
}

void r205_Em110AppearCheck()
{
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        if (pG->sceat_x17C & 0x20000000) {
            if (RsfCheck(G_ROOM_ID, 0)) {
                if ((u32) r205_AtHitCheckEmNum(0xC, -1) <= 1) {
                    RsfSet(G_ROOM_ID, 6);
                    r205_work.p->ems[4].em.setEm(0x6E, -1, 1, 1, 1);
                }
            }
        }
    }
}

void r205_Em111AppearCheck()
{
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        SceAtSetEnable(0x21, 1);
        if ((int) pG->flags_174 < 0) {
            if (RsfCheck(G_ROOM_ID, 0)) {
                if (r205_work.p->ems[6].dead == 1) {
                    Vec pos;

                    RsfSet(G_ROOM_ID, 7);
                    r205_work.p->ems[5].em.setEm(0x6F, -1, 1, 1, 1);
                    r205_work.p->ems[5].em.setFlag(1);
                    r205_work.p->ems[5].em.getPos(&pos);
                    ((cEmDoor*) r205_work.p->door0)->setOpen(&pos, 0, 0, 0);
                    SceAtSetEnable(0x21, 0);
                }
            }
        }
    }
}

static void r205_Em88Appear()
{
    RsfSet(G_ROOM_ID, 8);
    r205_work.p->ems[5].em.setEm(0x58, -1, 1, 1, 1);
    r205_work.p->ems[5].em.setFlag(1);
}

// Number of live enemies standing in the at areas `atA` / `atB` (-1: none).
int r205_AtHitCheckEmNum(int atA, int atB)
{
    int cnt = 0;
    u32 i;

    for (i = 0; i < 8; i++) {
        R205Em* e = &r205_work.p->ems[i];

        if (e->dead == 0) {
            if (e->em.isAlive() == 1) {
                if (SceAtCheckHitModel(atA, e->em.getPtr()) == 1) {
                    cnt++;
                } else if (atB != -1 && SceAtCheckHitModel(atB, e->em.getPtr()) == 1) {
                    cnt++;
                }
            }
        }
    }
    return cnt;
}

// Follows the four blades: the hit effects of the damage they take, the swing sound at the bottom
// of the arc, the at areas and the poles that rise while a blade is nearly vertical.
static void r205_PendulumMove()
{
    int at[4] = {6, 0xA, 0xB, 0x12};
    f32 x0 = r205_work.p->pole[0]->pos.x;
    int se[4] = {0, 0, 0, 0};
    u32 i;

    SceSleep(5);
    for (;;) {
        for (i = 0; i < 4; i++) {
            R205Pend* pd;
            f32 rot;

            if (r205_work.p->hit[i]->ckStatus() == 1) {
                switch (r205_work.p->hit[i]->dmWep) {
                case 7:
                case 8:
                case 0x21:
                    EmDmBloodSet2(r205_work.p->hit[i], 1, 0x13, 0, 0, 0);
                    break;
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 0x11:
                case 0x26:
                case 0x2B:
                    EmDmBloodSet2(r205_work.p->hit[i], 1, 0x12, 0, 0, 0);
                    break;
                }
            }
            pd = &r205_work.p->pend[i];
            pd->rotPrev = pd->rot;
            rot = pd->obj->rot.z;
            pd->rot = rot;
            if (fabsf(rot) <= 0.8f) {
                if (se[i] == 0) {
                    se[i] = 1;
                    RoomSeCall((u16) (12 + i), &r205_work.p->hit[i]->pParts->worldPos, 0, 0, 0);
                }
            } else {
                se[i] = 0;
            }
            if (fabsf(rot) <= 0.4f) {
                SceAtSetEnable(at[i], 1);
                if (r205_work.p->pole[i]) {
                    r205_work.p->pole[i]->be_flag |= 2;
                    r205_work.p->pole[i]->pos.x = rot / 0.1f * 700.0f + x0;
                    r205_work.p->pole[i]->matUpdate();
                }
            } else {
                SceAtSetEnable(at[i], 0);
                if (r205_work.p->pole[i]) {
                    r205_work.p->pole[i]->be_flag &= ~2;
                }
            }
        }
        SceSleep(1);
    }
}

// The player is hit by blade `p`: the death demo with a camera that keeps looking at him.
static void r205_ExecDieDemo(R205Pend* p)
{
    Camera cam;
    cModel* parts;
    Vec* wp;
    f32 d;
    int mot = 1;
    int i;

    if (pG->flags_68 & 0x00800000) {
        SceExit();
    }
    if (pG->flags_174 & 0x40000000) {
        SceExit();
    }
    BitOn(pG->flags_174, 0x40000000);
    BitOn(pG->flags_54, 0x40);
    BEGIN_EVENT(pPL, 0);
    d = p->rot - p->rotPrev;
    if (pPL->rot.y >= -1.5707964f && pPL->rot.y <= 1.5707964f) {
        pPL->rot.y = 0.0f;
        if (d > 0.0f) {
            mot = 0x41;
        }
    } else {
        pPL->rot.y = 3.1415927f;
        if (d < 0.0f) {
            mot |= 0x40;
        }
    }
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x20), 3, 0, (u16) mot, 0);
    wp = &pPL->getPartsPtr(2)->worldPos;
    RoomSeCall(0, wp, 0, 0, 0);
    PlSeCall(9, wp, 0, 0, 0);
    cam = pG->Cam;
    i = 0;
    parts = pPL->getPartsPtr(r205_camParts);
    while (!(MotionGetState(pPL) & 4)) {
        if (!(pG->flags_174 & 0x20000000)) {
            if (i == 2) {
                EstSet((int) pPL, -1, &pPL->getPartsPtr(2)->worldPos, 0, 1, 0x10, 0, 0, (u32) pPL, 0);
            }
            cam.param.at = parts->worldPos;
            i++;
            CameraSetOrientationUp(&cam);
            CamCtrl.x250 = (s32) &cam;
            SceSleep(1);
        } else {
            break;
        }
    }
    RoomSeCall(1, &pPL->getPartsPtr(2)->worldPos, 0, 0, 0);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x21), 3, 0, (u16) mot, 0);
    EstSet((int) pPL, -1, 0, 0, 1, 0x11, 0, 0, (u32) pPL, 0);
    DiedemoExec(0x19, 0);
    CamCtrl.Disable();
    CamCtrl.camera = cam;
    CamCtrl.cur = cam.param;
    CamSmth.ratio = 0.0f;
    CamCtrl.interp.frame = 0;
    CamCtrl.flags_28 |= 4;
}

static void r205_DrainEvent()
{
    SceEventStart(0);
    CamCtrl.CutCall(2);
    SceSleep(1);
    SceMesSet(0, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
    if (SceMesGetSelection() == 1) {
        void* zero;

        SndRoomStrVolSet(1, 0x15E);
        zero = NULL;
        RsfSet(G_ROOM_ID, 0);
        SceAtSetEnable(8, 0);
        SceAtSetEnable(7, 0);
        CamCtrl.CutCall(7);
        SndStrReq(1, 0, 0x80000003, 0, 0, 0.0f);
        EffectEspDelete(0, 0xE, 0, 0);
        EffectEspgenDelete(0, 0xE, 0);
        EffectEfmDelete(0, 0xE, 0);
        SetSstDispFlag(0xC, 0);
        SetSstDispFlag(0xD, 1);
        EstSet(0, -1, 0, 0, 1, 4, 1, 3, (u32) zero, zero);
        SceSetEventCancel(1, (TaskFunc) r205_DrainEventEnd, 0, -1, 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        r205_DrainEventEnd();
    } else {
        CamCtrl.Comeback(0);
        SceEventEnd(0);
    }
}

static void r205_DrainEventEnd()
{
    EffectEspDelete(1, 3, 0, 0);
    EffectEspgenDelete(1, 3, 0);
    EffectEfmDelete(1, 3, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    RsfSet(G_ROOM_ID, 1);
    r205_work.p->ems[0].em.setEm(0x68, -1, 1, 1, 1);
    SceAtSetEnable(0x11, 1);
    SndRoomStrVolReset(0x15E);
    SceAtSetEnable(0x15, 0);
    SceAtDataSet_exec(0x1B, 0x12, 0, (TaskFunc) r205_Em88Appear, 0, 1);
}

// Room stream: on while the player is found by an enemy.
static void r205_StrCheck()
{
    cEmWrap em;
    int playing = 0;

    for (;;) {
        int find = SceCkFindPL(NULL);

        if (pG->sceat_x17C & 0x02000000) {
            find = 0;
        }
        if (find == 1) {
            if (playing == 0) {
                SndRoomStrStart(1, 0, 1);
                playing = 1;
            }
        } else if (playing == 1) {
            SndRoomStrStop(3);
            playing = 0;
        }
        SceSleep(1);
    }
}

static void setTexRender()
{
    cObj* obj;
    u8* tbl = r205_texTbl;

    if (GetTexRenderMgr(&r205_work.p->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r205_work.p->tex->texId;
        r205_work.p->tex->repType = 1;
        EstSet(0, -1, 0, 0, 1, 1, r205_work.p->tex->mask | 1, 0, 0, 0);
    } else {
        pLog->err(0, 0, "R205Init() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0);
    obj->pInfo->setTexBlendTbl(tbl);
    obj->pInfo->setBlendRatio(0xFF);
    obj->pInfo->setBlendType(1);
    obj->x136 = 2;
    obj->x137 = 5;
    obj->x138 = 0x40;
}

static void r205_EnemyAppear()
{
    void* zero;

    while (PlGetStatus() == 0x80000) {
        SceSleep(1);
    }
    zero = NULL;
    RsfSet(G_ROOM_ID, 9);
    SceEventStart(0);
    BitOff(pG->flags_170, 0x10000000);
    BitOff(pG->flags_58, 0x40000000);
    pPL->setNoSuspend(1);
    EstSet(0, -1, 0, 0, 1, 6, 1, 3, (u32) zero, zero);
    CamCtrl.CutCall(9);
    SceSetEventCancel(1, (TaskFunc) r205_EnemyAppearEndProc, 0, -1, 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r205_EnemyAppearEndProc();
}

static void r205_EnemyAppearEndProc()
{
    EffectEspDelete(1, 3, 0, 0);
    EffectEspgenDelete(1, 3, 0);
    EffectEfmDelete(1, 3, 0);
    pPL->setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    setEm(0x70, -1, 1, 1, 1);
}

static void r205_RoomExitFunc()
{
    SndBgmTblSet(0x204, 1);
}

static void r205_TreasureBoxOpen(int id)
{
    OpenBoxMain(4, 0, 0x5B, id, -1, -1);
}

static void r205_TreasureBoxOpened(int id)
{
    OpenBoxMain(4, 1, 0x5B, id, -1, -1);
}

static void r205_ContinuePointSet()
{
    RsfSet(G_ROOM_ID, 12);
    GameSaveSave(&GameSave, pSaveData, -1);
}
