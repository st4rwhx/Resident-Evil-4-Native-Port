#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "event.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj1c.h"
#include "em.h"
#include "em_set.h"
#include "player.h"
#include "cam_ctrl.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "fade.h"
#include "flr_at.h"
#include "TexRender.h"
#include "rnd.h"

// Room 1-1B (D:/Bio4/Prog/r11b.cpp): the lake; the boat, the floating islands, the lake water
// rendered to texture, the Ganado ambush on the shore and the s00 event (Del Lago).

struct R11bWork {
    cEm* em[9];           // 0x00  the shore Ganado (7 in the event, 9 after EmSetChange)
    cEm* boat;            // 0x24  the boat (list entry 0x3C)
    TexRenderMng* tex[2]; // 0x28  water render targets
    u8 texTbl[2][0x80];   // 0x30  their blend tables (TexRenderModSet)
};

// The work pointer is a struct member: every store through the work reloads it.
struct R11bWorkPtr {
    R11bWork* p;
};

static R11bWorkPtr r11b_work;

// Pointer store through a reference: the pG load that follows stays below it.
static inline void PSet(cEm*& d, cEm* v) { d = v; }
// Scale set through references: the pG load of the following setMotion stays below the stores.
static inline void r11b_setScale(cObj* obj, f32 s)
{
    FSet(obj->scale.x, s);
    FSet(obj->scale.y, s);
    FSet(obj->scale.z, s);
}

// Hit effects of attribute type 2 (water)
static const AtEffInfo r11b_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

// The original TexRenderModRes reads the parts number from r4 although its prototype has one
// parameter (game/TexRender.cpp); the rooms pass it.
void TexRenderModResP(cModel* m, int parts) asm("TexRenderModRes");

static void R11b_bgm_ck();
static void r11b_ThunderFlagOn();
static void r11b_ThunderFlagOff();
static void r11b_ThunderMove();
extern "C" void EmSetChange();
static void r11b_EmEvent_exit();
static void r11b_EmEvent();
static void R11b_Event();
static void r11b_str_check();
extern "C" void Evt_R11BS00_Func(Event* e);
static void r11b_bort_pos_chk();

void R11bInit()
{
    Vec pos;
    Vec rot;
    Vec rot2;
    EmListData* l;
    cObj* obj = 0;   // the zero of the EstSet data arguments and the list entry's x3 (r27)
    int one = 1;     // COMPILER-DIFF: #13 (single use: update_equiv_regs moves the li next to the store)

    BitOn(pG->flags_54, 0x800);
    if (pG->x4F9F == 1) {
        RsfSet(G_ROOM_ID, 0);
    }
    R11bWork*& wp = r11b_work.p;   // the store's `lis` sits before the SceExec call (r30)
    SceExec(0x12, (TaskFunc) r11b_bort_pos_chk, 0, 0, 2, 0);
    BitOn(pG->flags_51BC, 8);
    // COMPILER-DIFF: candidate (sched1 issue-slot filler): the codeless asm depends on the flags
    // store (output dependence) and is issued in the idle cycle between it and the next pG reload,
    // so local-alloc's fake lifetimes of the two pG values no longer touch and both take r9 (the
    // original's `lwz r9; ... lwz r9`); without it the first load gets r11.
    asm("" : "=m"(rot2.x));
    BitOn(pG->flags_51BC, 2);
    BitOn(pG->flags_51C0, 0x01000000);
    BitOff(pG->door_flags_51CC, 0x8000);
    BitOff(pG->door_flags_51CC, 0x200);
    BitOff(pG->door_flags_51CC, 0x10);
#line 106 "D:/Bio4/Prog/r11b.cpp"
    wp = (R11bWork*) MEM_CALLOC(sizeof(R11bWork), 1, 0xd);
    EatMgr.registEffInfo(2, (AtEffInfo*) &r11b_eff_info);
    SceExec(0x12, (TaskFunc) r11b_ThunderMove, 0, 0, 2, 0);
    EvtMgr.SetFunc("evt_r11bs00_func", (void*) Evt_R11BS00_Func);
    EstSet((int) pPL, -1, 0, 0, 3, 2, 0x800, 0, 0, obj);
    EstSet((int) pPL, -1, 0, 0, 1, 2, 0x800, 0, 0, obj);
    BitOn(pG->flags_5010, 0x400);
    if (RsfCheck(G_ROOM_ID, 0)) {
        SceExec(0x12, (TaskFunc) R11b_bgm_ck, 0, 0, 2, 0);
    }
    l = EM_LIST(0x3C);
    l->x3 = 0;
    if (pG->room_id_prev == 0x10D && !(pG->flags_54 & 0x100)) {
        static const Vec r11b_boatPos0 = {141127.0f, -1299.0f, -57107.0f};
        static const Vec r11b_boatRot0 = {0.0f, -0.68f, 0.0f};

        // Copy-initialisation (placement new): the template loads are `mem/s/u` like a `Vec x = tbl;`
        // declaration, so sched2 does not chain them behind the frame stores through the twice-set
        // r29/r30 (an assignment `pos = tbl` goes through the synthesized operator= and loses /u).
        new (&pos) Vec(r11b_boatPos0);
        new (&rot) Vec(r11b_boatRot0);
        l->x3 = one;
        PSet(r11b_work.p->boat, EmSetFromList2(0x3C, 0));
        pG->room_id_prev = 0x11B;
        r11b_work.p->boat->setPos(&pos);
        r11b_work.p->boat->setAng(&rot);
    } else {
        r11b_work.p->boat = EmSetFromList2(0x3C, 0);
        l->x3 = 1;
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            static const Vec r11b_boatPos1 = {127560.0f, -1300.0f, 149100.0f};
            static const Vec r11b_boatRot1 = {0.0f, 3.0898211f, 0.0f};

            pos = r11b_boatPos1;
            rot2 = r11b_boatRot1;
            r11b_work.p->boat->setPos(&pos);
            r11b_work.p->boat->setAng(&rot2);
            if (RsfCheck(G_ROOM_ID, 0) == 0) {
                RsfSet(G_ROOM_ID, 0);
                SceExec(0x12, (TaskFunc) R11b_Event, 0, 0, 2, 0);
            }
        }
    }
    if (pG->room_id_prev == 0x11A) {
        RsfSet(G_ROOM_ID, 1);
        EmSetChange();
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) r11b_EmEvent, 0, 1);
        SceExec(0x12, (TaskFunc) r11b_str_check, 0, 2, 2, 0);
    }
    pos.x = 60782.0f;
    pos.y = -1300.0f;
    pos.z = 45761.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    {
        cObj* o = SetFloatIsland(ROOM_ARC_PTR(pG->pRoomArc, 0x1F), ROOM_ARC_PTR(pG->pRoomArc, 0x20), &pos, &rot);

        if (o) {
            r11b_setScale(o, 1.5f);
            ((cObj1c*) o)->setMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x21), ROOM_ARC_PTR(pG->pRoomArc, 0x22),
                                      ROOM_ARC_PTR(pG->pRoomArc, 0x23), ROOM_ARC_PTR(pG->pRoomArc, 0x24));
        }
    }
    pos.x = 12708.0f;
    pos.y = -1300.0f;
    pos.z = 33719.0f;
    rot.x = 0.0f;
    rot.y = 3.1415927f;
    rot.z = 0.0f;
    {
        cObj* o = SetFloatIsland(ROOM_ARC_PTR(pG->pRoomArc, 0x1F), ROOM_ARC_PTR(pG->pRoomArc, 0x20), &pos, &rot);

        if (o) {
            r11b_setScale(o, 2.0f);
            ((cObj1c*) o)->setMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x21), ROOM_ARC_PTR(pG->pRoomArc, 0x22),
                                      ROOM_ARC_PTR(pG->pRoomArc, 0x23), ROOM_ARC_PTR(pG->pRoomArc, 0x24));
        }
    }
    pos.x = 412.0f;
    pos.y = -1300.0f;
    pos.z = 64963.0f;
    rot.x = 0.0f;
    rot.y = 1.5707964f;
    rot.z = 0.0f;
    {
        cObj* o = SetFloatIsland(ROOM_ARC_PTR(pG->pRoomArc, 0x1F), ROOM_ARC_PTR(pG->pRoomArc, 0x20), &pos, &rot);

        if (o) {
            r11b_setScale(o, 1.7f);
            ((cObj1c*) o)->setMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x21), ROOM_ARC_PTR(pG->pRoomArc, 0x22),
                                      ROOM_ARC_PTR(pG->pRoomArc, 0x23), ROOM_ARC_PTR(pG->pRoomArc, 0x24));
        }
    }
    pos.x = 53338.0f;
    pos.y = -1300.0f;
    pos.z = 70475.0f;
    rot.x = 0.0f;
    rot.y = 0.7853982f;
    rot.z = 0.0f;
    {
        cObj* o = SetFloatIsland(ROOM_ARC_PTR(pG->pRoomArc, 0x1F), ROOM_ARC_PTR(pG->pRoomArc, 0x20), &pos, &rot);

        if (o) {
            r11b_setScale(o, 1.5f);
            ((cObj1c*) o)->setMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x21), ROOM_ARC_PTR(pG->pRoomArc, 0x22),
                                      ROOM_ARC_PTR(pG->pRoomArc, 0x23), ROOM_ARC_PTR(pG->pRoomArc, 0x24));
        }
    }
    TexRenderInit(&r11b_work.p->tex[0], 0xE0, 2);
    TexRenderInit(&r11b_work.p->tex[1], 0xE0, 2);
    FlrAtSetDefVal(0, 0, 3);
}

void R11bMain()
{
}

static void R11b_bgm_ck()
{
    SceSleep(1);
}

// Lightning on / off: the sky object's colour.
static void r11b_ThunderFlagOn()
{
    SmdGetObjPtr(0x2B)->pInfo->color[0] = 0xDA;
    SmdGetObjPtr(0x2B)->pInfo->color[1] = 0xF1;
    SmdGetObjPtr(0x2B)->pInfo->color[2] = 0xFF;
}

static void r11b_ThunderFlagOff()
{
    SmdGetObjPtr(0x2B)->pInfo->color[0] = 0x18;
    SmdGetObjPtr(0x2B)->pInfo->color[1] = 0x19;
    SmdGetObjPtr(0x2B)->pInfo->color[2] = 0x1A;
}

// Thunder every 90..235 frames.
static void r11b_ThunderMove()
{
    int cnt;

    SceSleep(1);
    {
        u8 r = Rnd() % 30;
        cnt = r * 5 + 90;
    }
    EffSetToolStateCallBack(0, r11b_ThunderFlagOn, r11b_ThunderFlagOff);
    for (;;) {
        if (cnt == 0) {
            EstSet(0, -1, 0, 0, 1, 4, 1, 0, 0, 0);
            {
                u8 r = Rnd() % 30;
                cnt = r * 5 + 90;
            }
            SceSndCallThunder();
        }
        cnt--;
        SceSleep(1);
    }
}

// Moves the shore Ganado list entries to the pier for the return from 1-1A.
#define EM_LIST_S(no) ((EmListData*) &pGS->emlist[(no) * 0x20])
extern "C" void EmSetChange()
{
    EmListData* l;

    l = EM_LIST_S(0x40);
    l->flags = 1;
    l->flags4 |= 0x40000000;
    l->pos[0] = -5972;
    l->pos[1] = 267;
    l->pos[2] = -1348;
    l->x3 = 0;
    l = EM_LIST_S(0x41);
    l->flags = 1;
    l->x3 = 0;
    l->pos[0] = -5582;
    l->pos[1] = 394;
    l->pos[2] = -1958;
    l = EM_LIST_S(0x3E);
    l->flags = 1;
    l->x3 = 0;
    l->pos[0] = -6060;
    l->pos[1] = 386;
    l->pos[2] = -2616;
    l = EM_LIST_S(0x3F);
    l->flags = 1;
    l->x3 = 0;
    l->pos[0] = -6440;
    l->pos[1] = 375;
    l->pos[2] = -2932;
}

static void r11b_EmEvent_exit()
{
    EmSetChange();
    EmMgr.destroy(r11b_work.p->em[0]);
    EmMgr.destroy(r11b_work.p->em[1]);
    EmMgr.destroy(r11b_work.p->em[2]);
    EmMgr.destroy(r11b_work.p->em[3]);
    EmMgr.destroy(r11b_work.p->em[4]);
    EmMgr.destroy(r11b_work.p->em[5]);
    EmMgr.destroy(r11b_work.p->em[6]);
    r11b_work.p->em[0] = EmSetFromList2(0x40, 1);
    r11b_work.p->em[1] = EmSetFromList2(0x41, 1);
    r11b_work.p->em[7] = EmSetFromList2(0x3E, 1);
    r11b_work.p->em[8] = EmSetFromList2(0x3F, 1);
    EffectEspDelete(1, 2, 0, 0);
    EffectEspgenDelete(1, 2, 0);
    EffectEfmDelete(1, 2, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 3: the Ganado ambush on the shore (camera cuts 3..6).
static inline void r11b_setPosXYZ(cModel* m, f32 x, f32 y, f32 z) { Vec v; v.x = x; v.y = y; v.z = z; m->setPos(&v); }
static inline void r11b_setAngXYZ(cModel* m, f32 x, f32 y, f32 z) { Vec v; v.x = x; v.y = y; v.z = z; m->setAng(&v); }

static void r11b_EmEvent()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        RsfSet(G_ROOM_ID, 1);
        r11b_work.p->em[0] = EmSetFromList2(0x40, 1);
        r11b_work.p->em[1] = EmSetFromList2(0x41, 1);
        r11b_work.p->em[2] = EmSetFromList2(0x42, 1);
        r11b_work.p->em[3] = EmSetFromList2(0x43, 1);
        r11b_work.p->em[4] = EmSetFromList2(0x44, 1);
        r11b_work.p->em[5] = EmSetFromList2(0x45, 1);
        r11b_work.p->em[6] = EmSetFromList2(0x46, 1);
        r11b_work.p->em[0]->setNoSuspend(1);
        r11b_work.p->em[1]->setNoSuspend(1);
        r11b_work.p->em[2]->setNoSuspend(1);
        r11b_work.p->em[3]->setNoSuspend(1);
        r11b_work.p->em[4]->setNoSuspend(1);
        r11b_work.p->em[5]->setNoSuspend(1);
        r11b_work.p->em[6]->setNoSuspend(1);
        SceEventStart(0);
        SndStrReq(1, 0x24, 0x80000003, 0, 0, 0.0f);
        pPL->setNoSuspend(1);
        r11b_setPosXYZ(pPL, -60735.0f, 2008.0f, -8455.0f);
        r11b_setAngXYZ(pPL, 0.0f, 2.64f, 0.0f);
        EstSet((int) r11b_work.p->em[0], -1, 0, 0, 1, 0xA, 1, 2, 0, 0);
        EstSet((int) r11b_work.p->em[1], -1, 0, 0, 1, 0xA, 1, 2, 0, 0);
        EstSet((int) r11b_work.p->em[2], -1, 0, 0, 1, 0xA, 1, 2, 0, 0);
        EstSet((int) r11b_work.p->em[3], -1, 0, 0, 1, 0xA, 1, 2, 0, 0);
        EstSet((int) r11b_work.p->em[4], -1, 0, 0, 1, 0xA, 1, 2, 0, 0);
        EstSet((int) r11b_work.p->em[5], -1, 0, 0, 1, 0xA, 1, 2, 0, 0);
        EstSet((int) r11b_work.p->em[6], -1, 0, 0, 1, 0xA, 1, 2, 0, 0);
        SceSetEventCancel(1, (TaskFunc) r11b_EmEvent_exit, 0, -1, 1);
        CamCtrl.CutCall(3);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(4);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(5);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(6);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        r11b_EmEvent_exit();
    }
}

// The s00 event on the first visit (skipped after the game was already saved here).
static void R11b_Event()
{
    int seen;

    SceSleep(1);
    seen = 0;
    BitOn(pG->flags_54, 0x400);
    if (pG->flags_54 & 0x40) {
        seen = 1;
    }
    BitOn(pG->flags_5010, 0x800);
    if (!(pG->flags_54 & 0x40)) {
        EvtMgr.EvtReadExec("event/evd/r11bs00.evd", 0, 4);
    }
    BitOff(pG->flags_54, 0x400);
    BitOff(pG->flags_5010, 0x800);
    SndBgmTblSet(0x11B, 1);
    SceExec(0x12, (TaskFunc) R11b_bgm_ck, 0, 0, 2, 0);
    if (seen == 0) {
        OpeSetOpenTerm(8, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (pG->flags_54 & 0x40) {
        SndRoomBgmStart(0, 30);
    }
}

// Battle stream while shore Ganado (type 0x22) are alive.
static void r11b_str_check()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        for (;;) {
            if (RsfCheck(G_ROOM_ID, 1) == 0) {
                SceSleep(1);
            } else {
                break;
            }
        }
        SndRoomStrStart(1, 0, 1);
        SceSleep(30);
        for (;;) {
            int n = 0;
            u32 i;

            for (i = 0; i < EmMgr.nArray; i++) {
                cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

                if (em->id == 0x22 && em->hp > 0 && (em->be_flag & 0x201) == 1) {
                    n++;
                }
            }
            if (n == 0) {
                break;
            }
            if (pG->flags_5010 & 0x00200000) {
                break;
            }
            SceSleep(1);
        }
        SndRoomStrStop(3);
    }
}

// 1 while the event is being skipped (EVT status bit 30).
static inline int r11b_evtSkip(Event* e)
{
    int skip = 1;

    if ((e->status & 0x40000000) == 0) {
        skip = 0;
    }
    return skip;
}

// Water render setup of the event's player stand-in: parts 6 (lake) and 7 / 8 (the two shores).
static inline void r11b_evtTexRenderSet(Event* e, void*& mod, int a, int b)
{
    if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
        TexRenderModSet((cModel*) mod, 6, r11b_work.p->texTbl[0], r11b_work.p->tex[0], 0, 0, 1, 1, 1.0f);
        TexRenderModSet((cModel*) mod, 7, r11b_work.p->texTbl[1], r11b_work.p->tex[1], 0, 0, 1, 1, 1.0f);
        TexRenderModSet((cModel*) mod, 8, r11b_work.p->texTbl[1], r11b_work.p->tex[1], 0, 0, 1, 1, 1.0f);
        ModelInfoRefrectOn((cModel*) mod, 6);
        ModelInfoRefrectOn((cModel*) mod, 7);
        ModelInfoRefrectOn((cModel*) mod, 8);
        ModelInfoSetTrans((cModel*) mod, 7, a);
        ModelInfoSetTrans((cModel*) mod, 8, b);
    }
}

static inline void r11b_evtEffDelete()
{
    EffectEspDelete(r11b_work.p->tex[0]->mask | 0x3001, 0, 0, 0);
    EffectEspgenDelete(r11b_work.p->tex[0]->mask | 0x3001, 0, 0);
    EffectEfmDelete(r11b_work.p->tex[0]->mask | 0x3001, 0, 0);
    EffectEspDelete(r11b_work.p->tex[1]->mask | 0x3001, 0, 0, 0);
    EffectEspgenDelete(r11b_work.p->tex[1]->mask | 0x3001, 0, 0);
    EffectEfmDelete(r11b_work.p->tex[1]->mask | 0x3001, 0, 0);
}

extern "C" void Evt_R11BS00_Func(Event* e)
{
    void* mod;

    switch (e->funcMode) {
    case 0:
        SmdSetTrans(0x7C, 0);
        break;
    case 1:
        SetSstAddAreaFlag(2);
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                int skip = r11b_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(0x80000002, 30, 0, 0);
                }
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag &= ~0x01000000;
                }
            }
            break;
        case 2:
            if (e->frame == 0x84) {
                int skip = r11b_evtSkip(e);

                if (skip == 0) {
                    EstSet(0, -1, 0, 0, 1, 0xB, 1, 0, 0, 0);
                }
            }
            break;
        case 3:
            if (e->frame == 0x55) {
                int skip = r11b_evtSkip(e);

                if (skip == 0) {
                    EstSet(0, -1, 0, 0, 1, 0xB, 1, 0, 0, 0);
                }
            }
            break;
        case 4:
            if (e->frame == 0x26) {
                int skip = r11b_evtSkip(e);

                if (skip == 0) {
                    EstSet(0, -1, 0, 0, 1, 0xB, 1, 0, 0, 0);
                }
            }
            break;
        case 5:
            if (e->frame == 0x5D) {
                int skip = r11b_evtSkip(e);

                if (skip == 0) {
                    EstSet(0, -1, 0, 0, 1, 0xB, 1, 0, 0, 0);
                }
            }
            break;
        case 8: {
            int skip = r11b_evtSkip(e);

            if (skip == 0) {
                SetNearClipDist(1.0f);
            }
            if (e->frame == 0x68) {
                int skip2 = r11b_evtSkip(e);

                if (skip2 == 0) {
                    EstSet(0, -1, 0, 0, 1, 0xB, 1, 0, 0, 0);
                }
            }
            break;
        }
        }
        switch (e->cut) {
        case 6:
            if (e->frame == 0) {
                r11b_evtTexRenderSet(e, mod, 1, 0);
                r11b_evtEffDelete();
                EstSet(0, -1, 0, 0, 1, 6, r11b_work.p->tex[1]->mask | 0x3001, 0, 0, 0);
            }
            break;
        case 7:
            if (e->frame == 0) {
                r11b_evtTexRenderSet(e, mod, 0, 1);
                r11b_evtEffDelete();
                EstSet(0, -1, 0, 0, 1, 7, r11b_work.p->tex[1]->mask | 0x3001, 0, 0, 0);
            }
            break;
        case 8:
            if (e->frame == 0) {
                r11b_evtTexRenderSet(e, mod, 1, 0);
                r11b_evtEffDelete();
                EstSet(0, -1, 0, 0, 1, 8, r11b_work.p->tex[1]->mask | 0x3001, 0, 0, 0);
                EstSet(0, -1, 0, 0, 1, 9, r11b_work.p->tex[0]->mask | 0x3001, 0, 0, 0);
            }
            break;
        default:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    TexRenderModResP((cModel*) mod, 6);
                    TexRenderModResP((cModel*) mod, 7);
                    TexRenderModResP((cModel*) mod, 8);
                }
                r11b_evtEffDelete();
            }
            break;
        }
        if (pG->costume2 == 1 && e->frame == 0) {
            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
                ModelInfoSetTrans((cModel*) mod, 8, 0);
            }
        }
        break;
    case 2:
        SetSstAddAreaFlag(0);
        SmdSetTrans(0x7C, 1);
        break;
    }
}

// Which pier the boat is nearer to when it stops decides the return position (room flag 2).
static void r11b_bort_pos_chk()
{
    int riding = 0;

    if (pG->flags_5010 & 0x00200000) {
        riding = 1;
    }
    for (;;) {
        if (riding) {
            if (!(pG->flags_5010 & 0x00200000)) {
                Vec pos = r11b_work.p->boat->pos;
                Vec pierA = {-49902.0f, -700.0f, 22743.0f};
                Vec pierB = {126064.0f, -700.0f, 148628.0f};
                f32 dA;
                f32 dB;

                riding = 0;
                dA = (pos.x - pierA.x) * (pos.x - pierA.x) + (pos.z - pierA.z) * (pos.z - pierA.z);
                dB = (pos.x - pierB.x) * (pos.x - pierB.x) + (pos.z - pierB.z) * (pos.z - pierB.z);
                if (dA < dB) {
                    RsfSet(G_ROOM_ID, 2);
                } else {
                    RsfClear(G_ROOM_ID, 2);
                }
            }
        } else if (pG->flags_5010 & 0x00200000) {
            riding = 1;
        }
        SceSleep(1);
    }
}
