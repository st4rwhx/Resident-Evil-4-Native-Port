#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "game.h"
#include "datactrl.h"
#include "dvd.h"
#include "read.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em10.h"
#include "emswitch.h"
#include "emBarred.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "motion.h"
#include "math_sub.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "item.h"
#include "sscrn.h"
#include "cSceObj.h"
#include "db_log.h"

// Room 2-01 (D:/Bio4/Prog/r201.cpp): the castle hall with the gem altar, the switch that opens the
// barred door, the claw-man ambush and the two bells above the hall.

struct R201Work {
    cEm* sw;             // 0x00
    cEm* barred;         // 0x04
    u8 effKind;          // 0x08
    u8 pad_9[3];
    u32 snd0;            // 0x0C
    u32 snd1;            // 0x10
    f32 doorY;           // 0x14  rest height of the battle-area door
    Vec sePos;           // 0x18
    Vec sePos2;          // 0x24
    cDataUnit* evd;      // 0x30
    cEmWrap em[3];       // 0x34
    cEmWrap em2[3];      // 0x58
    u8 pad_7C[4];
    cModel* gem[3];      // 0x80
    cObj* bell[2];       // 0x8C
    cSceObj altar;       // 0x94
    u32 strId;           // 0x18C
    u32 snd;             // 0x190
    int altarEff;        // 0x194
};

// The work pointer is a struct member: every store through the work reloads it.
struct R201WorkPtr {
    R201Work* p;
};

static R201WorkPtr r201_work;

static inline u32 BitCk(u32 f, u32 b) { return f & b; }

static void r201_openShelf(int no);
static void r201_openedShelf(int no);
static void r201_checkBellBreak();
static void r201_checkPicture();
static void r201_closeAltar_end();
static void r201_closeAltar();
void r201_initAltar();
void r201_moveAltarObj(int open, int init);
void r201_initGemObj();
int r201_setGem(int no);
static void r201_checkSetGem_end();
static void r201_checkSetGem();
int r201_checkAltarObj();
static void r201_checkAltar();
static void r201_checkDungeonKeyUse();
static void r201_checkDoor();
static void r201_closeBattleArea();
static void r201_setBattleArea_sub(int open);
void r201_setBattleArea(int open, int init);
static void r201_execEmReset_sub();
static void r201_execEmReset();
static void r201_disarmTrap_end();
static void r201_disarmTrap();
static void r201_execClawManUpCut_end();
static void r201_execClawManUpCut();
static void r201_appearClawMan();
void r201_setSwitchSe(int on);
void r201_setSwitchEnv(int on);
static void r201_checkSwitch(int on);
static void r201_execEvent00();
static void r201_execEvent00_sub();

void R201Init()
{
#line 52 "D:/Bio4/Prog/r201.cpp"
    R201Work*& wp = r201_work.p;   // reference: the following `lwz pG` stays below the store (r227 idiom)
    wp = (R201Work*) MEM_CALLOC(sizeof(R201Work), 1, 0xd);
    if (pG->x4F9F == 2) {
        RsfSet(G_ROOM_ID, 0);
        BitOn(pG->flags_51BC, 0x10000);
        RsfSet(G_ROOM_ID, 5);
        RsfSet(G_ROOM_ID, 4);
        SceExec(0x12, (TaskFunc) r201_execEmReset, 0, 0, 2, 0);
    }
    SceAtSetEnable(0, 0);
    SceAtSetEnable(1, 0);
    r201_initAltar();
    if (!(pG->flags_51C0 & 0x10000000)) {
        SceAtSetEnable(0x25, 0);
    } else {
        SceAtSetEnable(7, 0);
        SceAtSetEnable(0x25, 1);
        SceAtSetActColor(0x25, 1);
    }
    if (SceAtItemFlgCk(0x80) == 0) {
        cModel* m;

        SceAtDataSet_exec(0x20, 0x12, 0, (TaskFunc) r201_checkPicture, 0, 1);
        SceAtSetEnable(0x80, 1);
        m = SceAtItemModelPtr(0x80);
        if (m) {
            m->setNoSuspend(1);
        }
    }
    SceAtSetEnable(0x29, 0);
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceAtDataSet_exec(0xF, 0x12, 0, (TaskFunc) r201_execClawManUpCut, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        cEmWrap em;

        em.setEm(0x56, 2, 0, 1, 1);
        if (em.getPtr()) {
            ((cEmGanado*) em.getPtr())->setEvtMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x20), 0, ROOM_ARC_PTR(pG->pRoomArc, 0x21), 0);
        }
        SceAtDataSet_exec(0x10, 0x12, 0, (TaskFunc) r201_appearClawMan, 0, 1);
    } else {
        cObj* obj = SmdGetObjPtr(0x57);

        if (obj) {
            obj->be_flag &= ~2;
        }
        if ((pG->flags_54 & 0x100) || pG->room_id_prev == 0x203 || pG->room_id_prev == 0xFFF) {
            if (RsfCheck(G_ROOM_ID, 5)) {
                SceAtDataSet_exec(0xC, 0x12, 0, (TaskFunc) r201_execEmReset, 0, 1);
            }
        }
    }
    r201_work.p->doorY = SmdGetObjPtr(0x53)->pos.y;
    if (!(pGS->door_unlock[0] & 0x8000)) {
        r201_setBattleArea(0, 1);
        SceAtDataSet_exec(0xB, 0x12, 0, (TaskFunc) r201_checkDoor, 0, 1);
        SceExec(0x12, (TaskFunc) r201_checkDungeonKeyUse, 0, 0, 2, 0);
    } else {
        if (RsfCheck(G_ROOM_ID, 1)) {
            r201_setBattleArea(1, 1);
        } else {
            r201_setBattleArea(0, 1);
        }
    }
    getRoomEtcBarred(0x10, &r201_work.p->barred, 1);
    getRoomEtcSwitch(0x1C, &r201_work.p->sw, 1);
    if (r201_work.p->barred && r201_work.p->sw) {
        ((cEmSwitch*) r201_work.p->sw)->setOpenOnly();
        if (RsfCheck(G_ROOM_ID, 5) == 0) {
            ((cEmBarred*) r201_work.p->barred)->setClosed();
            ((cEmSwitch*) r201_work.p->sw)->setClosed();
            SceExec(0x12, (TaskFunc) r201_checkSwitch, 0, 0, 2, 0);
        } else {
            ((cEmBarred*) r201_work.p->barred)->setOpened();
            ((cEmSwitch*) r201_work.p->sw)->setClosed();
            SceAtSetEnable(0, 0);
            SceAtSetEnable(1, 0);
            SceAtSetEnable(3, 0);
            SceAtSetEnable(0x28, 0);
            SceAtSetEnable(5, 0);
        }
    }
    if (!(pG->flags_51BC & 0x10000)) {
        r201_work.p->evd = DC.setData(EvtMgr.NameChange("evd/r201s00.evd"));
        r201_work.p->evd->setCommand(2, 0, 0);
        EmReadSearch(0x1B, 0, r201_work.p->evd->size);
        SceAtDataSet_exec(8, 0x12, 0, (TaskFunc) r201_execEvent00, 0, 1);
    } else {
        EmReadSearch(0x1B, 0, 0);
    }
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        SceAtDataSet_exec(9, 0x12, 0, (TaskFunc) r201_execEvent00_sub, 0, 1);
    }
    // mid-function declarations (after the cEmWrap block): both bells share the two slots
    Vec pos;
    Vec rot;

    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        pos.x = 27273.0f;
        pos.y = 178.0f;
        pos.z = -33277.0f;
        rot.x = 0.0f;
        rot.y = 3.1415927f;
        rot.z = 0.0f;
        r201_work.p->bell[0] = SetObjBell(ROOM_ARC_PTR(pG->pRoomArc, 0x22), ROOM_ARC_PTR(pG->pRoomArc, 0x23), &pos, &rot);
    }
    if (RsfCheck(G_ROOM_ID, 11) == 0) {
        pos.x = 26972.0f;
        pos.y = 178.0f;
        pos.z = -50004.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        r201_work.p->bell[1] = SetObjBell(ROOM_ARC_PTR(pG->pRoomArc, 0x22), ROOM_ARC_PTR(pG->pRoomArc, 0x23), &pos, &rot);
    }
    SceExec(0x12, (TaskFunc) r201_checkBellBreak, 0, 0, 2, 0);
    SceSetItemEvent(0x26, 0x8F, 0xC, 0x10, (void (*)(int)) r201_openShelf, (void (*)()) r201_openedShelf, 0, 0);
    SceSetItemEvent(0x27, 0x90, 0xD, 0x11, (void (*)(int)) r201_openShelf, (void (*)()) r201_openedShelf, 1, 0);
    {
        u8 kind = 1;

        SceAtPtr(0x26)->x4A = kind;
        SceAtPtr(0x27)->x4A = kind;
    }
}

void R201Main()
{
}

// Shelf item events: the item is placed on the opened shelf.
void r201_openShelf_main(int no, int opened)
{
    cObj* obj;

    switch (no) {
    case 0:
        OpenBoxMain(0x15, opened, 0x15, 0x63, -1, -1);
        obj = SmdGetObjPtr(0x63);
        if (obj) {
            Vec* pa = &obj->rot;

            obj->pos.x = 4569.0f;
            obj->pos.y = 8756.0f;
            obj->pos.z = -44044.0f;
            obj->rot.x = 1.5707964f;
            obj->rot.y = -0.39f;
            obj->rot.z = 0.0f;
            obj->pParts->rot.x = 0.0f;
            obj->pParts->rot.y = 0.0f;
            obj->pParts->rot.z = 0.0f;
            obj->setPos(&obj->pos);
            obj->setAng(pa);
        }
        break;
    case 1:
        OpenBoxMain(0x16, opened, 0x15, 0x40, -1, -1);
        obj = SmdGetObjPtr(0x40);
        if (obj) {
            Vec* pa = &obj->rot;

            obj->pos.x = 24520.0f;
            obj->pos.y = 2000.0f;
            obj->pos.z = -22563.0f;
            obj->rot.x = -1.5707964f;
            obj->rot.y = -0.23f;
            obj->rot.z = 0.0f;
            obj->pParts->rot.x = 0.0f;
            obj->pParts->rot.y = 0.0f;
            obj->pParts->rot.z = 0.0f;
            obj->setPos(&obj->pos);
            obj->setAng(pa);
        }
        break;
    }
}

static void r201_openShelf(int no)
{
    r201_openShelf_main(no, 0);
}

static void r201_openedShelf(int no)
{
    r201_openShelf_main(no, 1);
}

// Disables a bell's area once it is broken.
static void r201_checkBellBreak()
{
    if (RsfCheck(G_ROOM_ID, 10)) {
        SceAtSetEnable(0x24, 0);
    }
    if (RsfCheck(G_ROOM_ID, 11)) {
        SceAtSetEnable(0x23, 0);
    }
    if (pSys->language) {
        SceAtSetEnable(0x24, 0);
        SceAtSetEnable(0x23, 0);
    }
    while (1) {
        if (RsfCheck(G_ROOM_ID, 10)) {
            if (RsfCheck(G_ROOM_ID, 11)) {
                break;
            }
        }
        if (RsfCheck(G_ROOM_ID, 10) == 0) {
            if (r201_work.p->bell[0] && ((cObjBell*) r201_work.p->bell[0])->ckBreak() == 1) {
                RsfSet(G_ROOM_ID, 10);
                SceAtSetEnable(0x24, 0);
            }
        }
        if (RsfCheck(G_ROOM_ID, 11) == 0) {
            if (r201_work.p->bell[1] && ((cObjBell*) r201_work.p->bell[1])->ckBreak() == 1) {
                RsfSet(G_ROOM_ID, 11);
                SceAtSetEnable(0x23, 0);
            }
        }
        SceSleep(1);
    }
}

static void r201_checkPicture()
{
    SceEventStart(0);
    LightMgr.onKind(0x7F);
    CamCtrl.CutCall(0xC);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceAtExecute(0x80);
    while (SceAtItemFlgCk(0x80) == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

static void r201_closeAltar_end()
{
    if (pG->flags_174 & 0x20000000) {
        SndStop(r201_work.p->snd, 0);
        EffectEspDelete(0, (u8) r201_work.p->altarEff, 0, 0);
        EffectEspgenDelete(0, (u8) r201_work.p->altarEff, 0);
        EffectEfmDelete(0, (u8) r201_work.p->altarEff, 0);
        r201_work.p->altar.setEndPos();
        SceAtSetEnable(0x1A, 1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

static void r201_closeAltar()
{
    RsfSet(G_ROOM_ID, 9);
    BitOff(pG->door_flags_51C8, 0x2000);
    pG->flags_174 &= ~0x20000000;
    SceSetEventCancel(1, (TaskFunc) r201_closeAltar_end, 0, 2, 1);
    SceEventStart(0);
    CamCtrl.CutCall(0xA);
    SceSleep(15);
    r201_moveAltarObj(0, 0);
    SceSleep(15);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceSetEventCancel(0, 0, 0, -1, 1);
    r201_closeAltar_end();
}

void r201_initAltar()
{
    if (RsfCheck(G_ROOM_ID, 9) == 0) {
        r201_moveAltarObj(1, 1);
        SceAtDataSet_exec(0x22, 0x12, 0, (TaskFunc) r201_closeAltar, 0, 1);
    } else {
        if (RsfCheck(G_ROOM_ID, 6) && RsfCheck(G_ROOM_ID, 7) && RsfCheck(G_ROOM_ID, 8)) {
            r201_moveAltarObj(1, 1);
            SceAtSetEnable(0x94, 0);
            SceAtSetEnable(0x95, 0);
            SceAtSetEnable(0x96, 0);
            return;
        }
        r201_moveAltarObj(0, 1);
    }
    r201_initGemObj();
    SceExec(0x12, (TaskFunc) r201_checkSetGem, 0, 0, 2, 0);
    SceAtDataSet_exec(0x1B, 0x12, 0, (TaskFunc) r201_checkAltar, 0, 1);
}

// A gem model rides along with the altar (first free sub slot).
static inline void r201_attachGem(R201Work* w, cModel* g)
{
    cSceObj* a = &w->altar;

    if (g) {
        u32 i = 0;

        // a goto loop (no loop notes): `i * 4` is recomputed per iteration.
        // The `do { } while (0)` around the increment block only (the label INSIDE the notes)
        // puts that block at loop depth 2, so `i` gets 8 weighted refs and is allocated first
        // (r9); `a->sub + 8`/`i * 4`/work then take r11/r10/r11 as in the original.
        if (w->altar.sub[0] == 0) {
            w->altar.sub[0] = g;
        } else {
            do {
            next:
                i++;
                if (i > 3) {
                    return;
                }
            } while (0);
            if (a->sub[i] != 0) {
                goto next;
            }
            a->sub[i] = g;
        }
    }
}

// Lowers (`open` 1) or raises the altar; `init` 1 sets it up without the animation.
void r201_moveAltarObj(int open, int init)
{
    cObj* obj;

    obj = SmdGetObjPtr(0x5C);
    if (obj) {
        Vec d = {0.0f, -6012.0f, 0.0f};

        if (init == 1) {
            r201_work.p->altarEff = EspPullCoreKind();
            r201_work.p->altar.initMove1_pos(obj, 120, &d, 10.0f, 10.0f);
            r201_work.p->altar.setVibration(10, 10, 2.0f, 0.5f, 2.0f);
            if (open == 1) {
                SceAtSetEnable(0x1A, 0);
                r201_work.p->altar.setReverse(1);
            } else {
                SceAtSetEnable(0x1A, 1);
                r201_work.p->altar.setReverse(0);
            }
        } else {
            r201_work.p->snd = SndCall(6, 0x12, 0, 0, 0, 0);
            if (open == 1) {
                r201_attachGem(r201_work.p, r201_work.p->gem[0]);
                r201_attachGem(r201_work.p, r201_work.p->gem[1]);
                r201_attachGem(r201_work.p, r201_work.p->gem[2]);
                r201_work.p->altar.setReverse(0);
                EstSet(0, -1, 0, 0, 1, 8, 1, (u8) r201_work.p->altarEff, 0, 0);
            } else {
                r201_work.p->altar.setReverse(1);
                EstSet(0, -1, 0, 0, 1, 9, 1, (u8) r201_work.p->altarEff, 0, 0);
            }
            // ONE loop for both directions: the `SceSleep(1)` block is shared and the `open` compare
            // (kept in r29 by mfcr) is re-tested per iteration after the sleep (`b test; sleep: ..; test:`).
            for (;;) {
                if (open == 1) {
                    if (r201_work.p->altar.move() == 0) {
                        SceAtSetEnable(0x1A, 0);
                        break;
                    }
                } else {
                    if (r201_work.p->altar.move() == 0) {
                        SceAtSetEnable(0x1A, 1);
                        break;
                    }
                }
                SceSleep(1);
            }
            SndCall(6, 0x13, 0, 0, 0, 0);
        }
    }
}

// The three gem items on the altar.
void r201_initGemObj()
{
    SceAtSetEnable(0x94, 1);
    SceAtSetEnable(0x95, 1);
    SceAtSetEnable(0x96, 1);
    r201_work.p->gem[0] = SceAtItemModelPtr(0x94);
    r201_work.p->gem[1] = SceAtItemModelPtr(0x95);
    r201_work.p->gem[2] = SceAtItemModelPtr(0x96);
    r201_work.p->gem[0]->pos.x = 141.0f;
    r201_work.p->gem[0]->pos.y = 3908.0f;
    r201_work.p->gem[0]->pos.z = -31679.0f;
    r201_work.p->gem[1]->pos.x = 517.0f;
    r201_work.p->gem[1]->pos.y = 3908.0f;
    r201_work.p->gem[1]->pos.z = -31679.0f;
    r201_work.p->gem[2]->pos.x = 858.0f;
    r201_work.p->gem[2]->pos.y = 3908.0f;
    r201_work.p->gem[2]->pos.z = -31679.0f;
    r201_work.p->gem[0]->setNoSuspend(1);
    r201_work.p->gem[1]->setNoSuspend(1);
    r201_work.p->gem[2]->setNoSuspend(1);
    r201_work.p->gem[0]->lightInfo.x50 &= ~0x20;
    r201_work.p->gem[0]->lightInfo.x50 |= 0x10;
    r201_work.p->gem[1]->lightInfo.x50 &= ~0x20;
    r201_work.p->gem[1]->lightInfo.x50 |= 0x10;
    r201_work.p->gem[2]->lightInfo.x50 &= ~0x20;
    r201_work.p->gem[2]->lightInfo.x50 |= 0x10;
    SceAtSetEnable(0x94, 0);
    SceAtSetEnable(0x95, 0);
    SceAtSetEnable(0x96, 0);
    if (RsfCheck(G_ROOM_ID, 6)) {
        SceAtSetEnable(0x94, 1);
    }
    if (RsfCheck(G_ROOM_ID, 7)) {
        SceAtSetEnable(0x95, 1);
    }
    if (RsfCheck(G_ROOM_ID, 8)) {
        SceAtSetEnable(0x96, 1);
    }
}

// Puts gem `no` on the altar; 1 when all three are in place.
int r201_setGem(int no)
{
    int done = 0;

    SceEventStart(0);
    CamCtrl.CutCall(0xD);
    SceSleep(15);
    SndCall(6, 0x14, 0, 0, 0, 0);
    switch ((u32) no) {
    case 0:
        RsfSet(G_ROOM_ID, 6);
        SceAtSetEnable(0x94, 1);
        break;
    case 1:
        RsfSet(G_ROOM_ID, 7);
        SceAtSetEnable(0x95, 1);
        break;
    case 2:
        RsfSet(G_ROOM_ID, 8);
        SceAtSetEnable(0x96, 1);
        break;
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    if (r201_checkAltarObj() == 1) {
        done = 1;
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    return done;
}

static void r201_checkSetGem_end()
{
    if (pG->flags_174 & 0x20000000) {
        SndStop(r201_work.p->snd, 0);
        EffectEspDelete(0, (u8) r201_work.p->altarEff, 0, 0);
        EffectEspgenDelete(0, (u8) r201_work.p->altarEff, 0);
        EffectEfmDelete(0, (u8) r201_work.p->altarEff, 0);
        r201_work.p->altar.setEndPos();
        SceAtSetEnable(0x1A, 0);
    }
    if (r201_work.p->gem[0]) {
        r201_work.p->gem[0]->be_flag &= ~2;
    }
    if (r201_work.p->gem[1]) {
        r201_work.p->gem[1]->be_flag &= ~2;
    }
    if (r201_work.p->gem[2]) {
        r201_work.p->gem[2]->be_flag &= ~2;
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    BitOn(pG->flags_51C4, 0x04000000);
    pG->door_flags_51C8 |= 0x2000;
}

// Waits for the gems to be used and lowers the altar once all three are set.
static void r201_checkSetGem()
{
    for (;;) {
        if (ItemMgr.check(0x1E) == 1) {
            if (r201_setGem(0) == 1) {
                break;
            }
        }
        if (ItemMgr.check(0x1F) == 1) {
            if (r201_setGem(1) == 1) {
                break;
            }
        }
        if (ItemMgr.check(0x39) == 1) {
            if (r201_setGem(2) == 1) {
                break;
            }
        }
        SceSleep(1);
    }
    SceAtSetEnable(0x1B, 0);
    pG->flags_174 &= ~0x20000000;
    SceEventStart(0);
    CamCtrl.CutCall(0xE);
    SceSleep(15);
    SceSetEventCancel(1, (TaskFunc) r201_checkSetGem_end, 0, 2, 1);
    r201_moveAltarObj(1, 0);
    SceSleep(15);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r201_checkSetGem_end();
}

// Counts the gems on the altar; 1 when all three are set (a message otherwise).
int r201_checkAltarObj()
{
    int n = 0;

    if (RsfCheck(G_ROOM_ID, 6)) {
        n = 1;
    }
    if (RsfCheck(G_ROOM_ID, 7)) {
        n++;
    }
    if (RsfCheck(G_ROOM_ID, 8)) {
        n++;
    }
    switch ((u32) n) {
    case 0:
        SceMesSet(3, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
        break;
    case 1:
        SceMesSet(4, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
        break;
    case 2:
        SceMesSet(4, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
        break;
    case 3:
        return 1;
    }
    return 0;
}

static void r201_checkAltar()
{
    SceEventStart(0);
    CamCtrl.CutCall(0xD);
    SceSleep(15);
    r201_checkAltarObj();
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    if (ItemMgr.num(0x1E) != 0 || ItemMgr.num(0x1F) != 0 || ItemMgr.num(0x39) != 0) {
        SubScreenOpen(0x80, 1);
    }
    SceEventEnd(0);
}

// The dungeon key unlocks the battle-area door.
static void r201_checkDungeonKeyUse()
{
    while (ItemMgr.check(0xC3) != 1) {
        SceSleep(1);
    }
    SceEventStart(0);
    pPL->setNoSuspend(1);
    if (pSUB) {
        pSUB->setNoSuspend(1);
    }
    pG->door_unlock[0] |= 0x8000;
    SceAtSetEnable(0xB, 0);
    SceUpCut(1, -1, 1, 0);
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    SceEventEnd(0);
    RsfSet(G_ROOM_ID, 1);
    BitOn(pG->door_flags_51CC, 0x40000000);
    GameSaveSave(&GameSave, pSaveData, -1);
    SceSleep(12);
    r201_setBattleArea(1, 0);
}

static void r201_checkDoor()
{
    SceUpCut(0, -1, 0, 4);
    if (ItemMgr.num(0xC3) == 0) {
        CamCtrl.Comeback(0);
    } else {
        SubScreenOpen(0x80, 1);
    }
}

static void r201_closeBattleArea()
{
    SceEventStart(1);
    CamCtrl.CutCall(5);
    SceSleep(15);
    r201_setBattleArea(0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSleep(15);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The door's collision follows the door once it is high enough.
static void r201_setBattleArea_sub(int open)
{
    if (open == 1) {
        cObj* obj;

        RsfSet(G_ROOM_ID, 1);
        pG->door_flags_51CC |= 0x40000000;
        obj = SmdGetObjPtr(0x53);
        while (obj->pos.y - r201_work.p->doorY < 1600.0f) {
            SceSleep(1);
        }
        SceAtSetEnable(0xA, 0);
    } else {
        RsfClear(G_ROOM_ID, 1);
        pG->door_flags_51CC &= ~0x40000000;
        SceAtSetEnable(0xA, 1);
    }
}

// Raises (`open` 1) or lowers the battle-area door; `init` 1 places it without animation.
void r201_setBattleArea(int open, int init)
{
    cObj* obj;

    if (open == 1) {
        if (pG->flags_174 & 0x40000000) {
            return;
        }
        pG->flags_174 |= 0x40000000;
    } else {
        if (!(pG->flags_174 & 0x40000000)) {
            return;
        }
        pG->flags_174 &= ~0x40000000;
    }
    obj = SmdGetObjPtr(0x53);
    obj->be_flag |= 0x20;
    if (init == 1) {
        if (open == 1) {
            SceAtSetEnable(0xA, 0);
            obj->pos.y += 2600.0f;
        } else {
            SceAtSetEnable(0xA, 1);
        }
    } else {
        // pool order: the two steps before the 0.0 the speed starts from
        const f32 step = 57.77778f;
        const f32 acc = 20.0f;
        f32 spd = 0.0f;

        if (open == 1) {
            SndCall(6, 0x24, 0, 0, 0, 0);
        } else {
            SndCall(6, 0x26, 0, 0, 0, 0);
        }
        SceExec(0x12, (TaskFunc) r201_setBattleArea_sub, open, 0, 2, 0);
        if (open == 1) {
            EstSet(0, -1, 0, 0, 1, 0xA, 1, 0, 0, 0);
        } else {
            EstSet(0, -1, 0, 0, 1, 0xB, 1, 0, 0, 0);
        }
        for (;;) {
            if (open == 1) {
                obj->pos.y += step;
                if (r201_work.p->doorY + 2600.0f < obj->pos.y) {
                    obj->pos.y = r201_work.p->doorY + 2600.0f;
                    SndCall(6, 0x25, 0, 0, 0, 0);
                    return;
                }
            } else {
                obj->pos.y -= spd;
                spd += acc;
                if (r201_work.p->doorY > obj->pos.y) {
                    obj->pos.y = r201_work.p->doorY;
                    SndCall(6, 0x27, 0, 0, 0, 0);
                    return;
                }
            }
            SceSleep(1);
        }
    }
}

// The reset wave: three enemies walk to the hall once the player enters the side areas.
static void r201_execEmReset_sub()
{
    Vec pos;

    while (SceAtHitCheck(0x12) != 1 && SceAtHitCheck(0x16) != 1) {
        SceSleep(1);
    }
    r201_work.p->em[0].setPtr(0x58, 2, 0);
    r201_work.p->em[1].setPtr(0x59, 2, 0);
    r201_work.p->em[2].setPtr(0x5A, 2, 0);
    r201_work.p->em[2].setFlag(1);
    SceSleep(60);
    AreaGetCenterPos(&pos, &SceAtPtr(0x13)->area);
    r201_work.p->em[0].setGoto(&pos, 1);
    SceSleep(15);
    AreaGetCenterPos(&pos, &SceAtPtr(0x14)->area);
    r201_work.p->em[1].setGoto(&pos, 1);
    SceSleep(150);
    while (r201_work.p->em[2].isAlive() == 1 && ((cEmGanado*) r201_work.p->em[2].getPtr())->ckBombFire()) {
        SceSleep(1);
    }
    AreaGetCenterPos(&pos, &SceAtPtr(0x15)->area);
    r201_work.p->em[2].setGoto(&pos, 1);
}

static void r201_execEmReset()
{
    Vec pos;

    r201_work.p->em2[0].setPtr(0x57, 2, 0);
    r201_work.p->em2[1].setPtr(0x5B, 2, 0);
    r201_work.p->em2[2].setPtr(0x5C, 2, 0);
    SceExec(0x12, (TaskFunc) r201_execEmReset_sub, 0, 0, 2, 0);
    AreaGetCenterPos(&pos, &SceAtPtr(0xD)->area);
    r201_work.p->em2[0].setGoto(&pos, 2);
    while (r201_work.p->em2[0].ckGoto() != 0) {
        SceSleep(1);
    }
    AreaGetCenterPos(&pos, &SceAtPtr(0x18)->area);
    r201_work.p->em2[1].setGoto(&pos, 0xB);
    r201_work.p->em2[2].setGoto(&pos, 0xB);
    SceSleep(30);
    AreaGetCenterPos(&pos, &SceAtPtr(0x17)->area);
    r201_work.p->em2[0].setGoto(&pos, 1);
    while (r201_work.p->em2[0].ckGoto() != 0) {
        SceSleep(1);
    }
    r201_work.p->em2[0].clearFindPL();
    r201_work.p->em2[0].setFlag(0x10);
}

static void r201_disarmTrap_end()
{
    if (!(pG->flags_174 & 0x10000000)) {
        r201_setSwitchSe(1);
    }
    if (!(pG->flags_174 & 0x08000000)) {
        r201_setSwitchEnv(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    pPL->setNoSuspend(0);
    if (RsfCheck(G_ROOM_ID, 0)) {
        SceAtDataSet_exec(0xC, 0x12, 0, (TaskFunc) r201_execEmReset, 0, 1);
    }
}

// The switch opens the barred door.
static void r201_disarmTrap()
{
    pPL->setNoSuspend(1);
    r201_work.p->sw->setNoSuspend(1);
    SceSetEventCancel(1, (TaskFunc) r201_disarmTrap_end, 0, -1, 1);
    SceEventStart(1);
    CamCtrl.CutCall(6);
    pG->flags_174 |= 0x10000000;
    r201_setSwitchSe(1);
    SndCall(6, 0xC, 0, 0, 0, 0);
    SndCall(6, 0xD, 0, 0, 0, 0);
    SceSleep(60);
    pG->flags_174 |= 0x08000000;
    r201_setSwitchEnv(1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r201_disarmTrap_end();
}

static void r201_execClawManUpCut_end()
{
    cEmWrap em;

    em.setPtr(0x56, -1, 1);
    em.setNoSuspend(0);
    SndStrReq(r201_work.p->strId, 4, 200, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

static void r201_execClawManUpCut()
{
    SceSetEventCancel(1, (TaskFunc) r201_execClawManUpCut_end, 0, -1, 1);
    SceEventStart(0);
    RsfSet(G_ROOM_ID, 2);
    cEmWrap em;

    em.setPtr(0x56, 2, 0);
    em.setNoSuspend(1);
    r201_work.p->strId = SndStrReq(0, 0x1E, 0x80000003, 0, 0, 0.0f);
    CamCtrl.CutCall(9);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(7);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r201_execClawManUpCut_end();
}

// The claw man drops into the hall; the battle-area door closes behind the player.
static void r201_appearClawMan()
{
    cEmWrap em;
    cObj* obj;

    em.setPtr(0x56, 2, 0);
    em.setFlag(1);
    EstSet(0, -1, 0, 0, 1, 4, 0, 0, 0, 0);
    SndRoomStrStart(1, 0, 1);
    SceAtSetEnable(0x29, 1);
    SceAtDataSet_exec(0x11, 0x12, 0, (TaskFunc) r201_closeBattleArea, 0, 1);
    SceSleep(17);
    obj = SmdGetObjPtr(0x57);
    if (obj) {
        obj->be_flag &= ~2;
    }
    while (em.isActive() == 1) {
        int trg = DebugTrg(0);

        if (trg == 1) {
            if (em.getPtr()) {
                em.getPtr()->hp = trg;
            }
        }
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 0);
    if (!(pG->flags_174 & 0x40000000)) {
        SceEventStart(0);
        CamCtrl.CutCall(5);
        SceSleep(15);
        r201_setBattleArea(1, 0);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSleep(15);
        CamCtrl.Comeback(0);
        SceEventEnd(0);
    }
    SceAtSetEnable(0x29, 0);
    SceAtSetEnable(0x11, 0);
    SndRoomStrStop(3);
    if (RsfCheck(G_ROOM_ID, 5)) {
        SceAtDataSet_exec(0xC, 0x12, 0, (TaskFunc) r201_execEmReset, 0, 1);
    }
}

// Switch sounds: `on` 1 starts the two loops, 0 stops them.
void r201_setSwitchSe(int on)
{
    r201_work.p->sePos.x = 41100.0f;
    r201_work.p->sePos.y = 3500.0f;
    r201_work.p->sePos.z = -41000.0f;
    r201_work.p->sePos2.x = 41100.0f;
    r201_work.p->sePos2.y = 3500.0f;
    r201_work.p->sePos2.z = -41000.0f;
    if (on == 1) {
        if (r201_work.p->snd0) {
            SndStop(r201_work.p->snd0, 0);
            SndCall(6, 0xC, &r201_work.p->sePos, 0, 0, 0);
            r201_work.p->snd0 = 0;
        }
        if (r201_work.p->snd1) {
            SndStop(r201_work.p->snd1, 0);
            SndCall(6, 0xD, &r201_work.p->sePos, 0, 0, 0);
            r201_work.p->snd1 = 0;
        }
    } else {
        r201_work.p->snd0 = SndCall(6, 0xA, &r201_work.p->sePos, 0, 0, 0);
        r201_work.p->snd1 = SndCall(6, 0xB, &r201_work.p->sePos, 0, 0, 0);
    }
}

// Switch state: `on` 1 opens the barred door and disables the trap areas.
void r201_setSwitchEnv(int on)
{
    void* zero;

    r201_work.p->sePos.x = 41100.0f;
    r201_work.p->sePos.y = 3500.0f;
    r201_work.p->sePos.z = -41000.0f;
    r201_work.p->sePos2.x = 41100.0f;
    r201_work.p->sePos2.y = 3500.0f;
    r201_work.p->sePos2.z = -41000.0f;
    if (on == 1) {
        zero = 0;
        RsfSet(G_ROOM_ID, 5);
        ((cEmBarred*) r201_work.p->barred)->setOpen(0);
        EffectEspgenDelete(0, r201_work.p->effKind, 0);
        EstSet(0, -1, 0, 0, 1, 3, 1, 0, (u32) zero, zero);
        SceAtSetEnable(0, 0);
        SceAtSetEnable(1, 0);
        SceAtSetEnable(3, 0);
        SceAtSetEnable(0x28, 0);
        SceAtSetEnable(5, 0);
    } else {
        zero = 0;
        RsfClear(G_ROOM_ID, 5);
        ((cEmBarred*) r201_work.p->barred)->setClose(0);
        EstSet(0, -1, 0, 0, 1, 0, 1, r201_work.p->effKind, (u32) zero, zero);
        SceAtSetEnable(3, 1);
        SceAtSetEnable(0x28, 1);
        SceAtSetEnable(5, 1);
        if (SceAtHitCheck(2) == 0) {
            SceAtSetEnable(0, 1);
            SceAtPtr(3)->dstAngle = 0.0f;
            SceAtPtr(0x28)->dstAngle = 0.0f;
        } else {
            SceAtSetEnable(1, 1);
            SceAtPtr(3)->dstAngle = 3.1415927f;
            SceAtPtr(0x28)->dstAngle = 3.1415927f;
        }
    }
}

static void r201_checkSwitch(int on)
{
    r201_work.p->effKind = EspPullCoreKind();
    r201_setSwitchSe(on);
    r201_setSwitchEnv(on);
    while (on != 1) {
        if (r201_work.p->sw && ((cEmSwitch*) r201_work.p->sw)->ckOpen() == 1) {
            SndCall(6, 0x23, 0, 0, 0, 0);
            SceExec(0x12, (TaskFunc) r201_disarmTrap, 0, 0, 2, 0);
            break;
        }
        SceSleep(1);
        // COMPILER-DIFF: #13 -- loop.c hoists the invariant `on != 1` compare into a CC pseudo (mfcr/mtcrf
        // across the calls); the original re-compares `cmpwi r31,1` at the loop bottom (the REG_EQUIV compare
        // is re-materialised at its use). The launder makes `on` loop-variant so the compare stays.
        asm("" : "+r"(on));
    }
}

// The entrance event: plays the evd once its data is loaded.
static void r201_execEvent00()
{
    ReadModule* m;
    u32 key;

    BitOn(pG->flags_51BC, 0x10000);
    SceEventStart(0);
    if (r201_work.p->evd->waitLoadOk() == 1) {
        pG->flags_54 |= 0x400;
        SceSleep(2);
        m = SearchEmModule(0x1B);
        MemorySwap(m->pArc, (u32) r201_work.p->evd->addr, r201_work.p->evd->size);
        EvtMgr.SetEvt(m->pArc, &key);
        ((Event*) key)->status |= 0x800;
        while (EvtMgr.IsAliveEvt(&EvtMgr.x34, 0, 0) != 0) {
            SceSleep(1);
        }
        MemorySwap(m->pArc, (u32) r201_work.p->evd->addr, r201_work.p->evd->size);
    }
    r201_work.p->evd->setCommand(4, 0, 0);
    SceEventEnd(0);
    OpeOwTypeSet(4);
}

static void r201_execEvent00_sub()
{
    int i;

    RsfSet(G_ROOM_ID, 4);
    SndCall(6, 3, 0, 0, 0, 0);
    for (i = 0; i < 120; i++) {
        if (pG->flags_51BC & 0x10000) {
            SceExit();
        }
        SceSleep(1);
    }
    SndCall(6, 3, 0, 0, 0, 0);
}
