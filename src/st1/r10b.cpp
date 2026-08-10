#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "event.h"
#include "dmg.h"
#include "flag_rsf.h"
#include "st_mgr_event.h"
#include "global.h"
#include "game.h"
#include "datactrl.h"
#include "dvd.h"
#include "read.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj1c.h"
#include "obj16.h"
#include "em.h"
#include "em_set.h"
#include "esp.h"
#include "est.h"
#include "espgen.h"
#include "player.h"
#include "cockpit.h"
#include "cam_ctrl.h"
#include "cam_extra.h"
#include "snd.h"
#include "rnd.h"

// Room 1-0b (D:/Bio4/Prog/r10b.cpp): the lake; the boss fight from the boat (enemy 0x2f), the
// floating islands, the binocular view of the cliff event and the boss's tentacle heads.

struct R10bWork {
    cEm* boss;             // 0x00  the lake boss (list entry 0xA1)
    cEm* boat;             // 0x04  the boat enemy (list entry 0xA2 / 0xA3)
    cEm* em0;              // 0x08  list entry 0xA0
    int count;             // 0x0C  action button count of the s20 event
    cDataUnit* evt[16];    // 0x10  event data units by readEvent index
    IdBinocular* bino;     // 0x50
    FocusAnimation* focus; // 0x54
    u8 pad_58[4];
    IdBinocular binoObj;   // 0x5C
    FocusAnimation focusObj;  // 0x9C
    cObj* island[4];       // 0xAC
    u8 pad_BC[4];
    cObj* head[6];         // 0xC0  tentacle heads (obj 0x16)
};

static R10bWork* r10b_work;

// readEvent stores into the event unit table and reloads both the work pointer and the entry after
// it: the pointer is read through a one-member struct view there (see r10f).
struct R10bWorkPtr {
    R10bWork* p;
};
#define R10B_WORK (((R10bWorkPtr*) &r10b_work)->p)

// Hit effects of attribute type 2 (water)
static const AtEffInfo r10b_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

// The lake boss (enemy 0x2f) by vtable slot.
class cEm2f : public cEm {
public:
    virtual void v50();
    virtual void setCamPos(Vec* pos, f32 ang);   // 0x58
    virtual void setDie();                        // 0x60
};

// Pointer store through a reference: the work pointer is reloaded after it (see st_room.h).
static inline void PSet(cEm*& d, cEm* v) { d = v; }
static inline void PSet(IdBinocular*& d, IdBinocular* v) { d = v; }
static inline void PSet(FocusAnimation*& d, FocusAnimation* v) { d = v; }

// The running event's key (&EvtMgr.x34 as an accessor result: the address is formed last).
static inline u32* evtKey(EventMgr* m) { return &m->x34; }

// Waits for the running event to end.
static inline void r10b_waitEvt()
{
    // `&EvtMgr` inside the loop (no pointer local before it): loop.c hoists the `addi` into the
    // inner preheader from its own `lis` (the target's second EvtMgr high, r26).
    while (EvtMgr.IsAliveEvt(evtKey(&EvtMgr), 0, 0)) {
        SceSleep(1);
    }
}

// The rooms call Event::FlgOnStatus out of line (event.h has it in-class).
void EvtFlgOnStatus(Event* e, u32 no) asm("FlgOnStatus__5EventUl");

extern "C" int readEvent(int no, int wait, void** out);
extern "C" void freeEvent(int no);
static void R10b_chkEmDie();
static void R10b_chkWater();
static void r10b_GakeEvent();
extern "C" void Evt_R10BS00_Func(Event* e);
// The room passes `li r4,0` to the parameterless IdBinocular::cutin (old prototype, r214 idiom).
void IdBinocularCutinI(IdBinocular*, int) asm("cutin__11IdBinocular");
extern "C" void em2fTentacleMove(cEm* em, Event* e, int mode);
extern "C" void Evt_R10BS10_Func(Event* e);
extern "C" void Evt_R10BS20_Func(Event* e);
extern "C" void Evt_R10BS21_Func(Event* e);
extern "C" void Evt_R10BS22_Func(Event* e);
extern "C" void Evt_R10BSXX_Func_Pl0f(Event* e);
extern "C" void Evt_R10BSXX_Func_Em2f(Event* e);
static void r10b_setEm();

void R10bInit()
{
    void* zero = 0;
    Vec pos;
    Vec rot;
    cObj* obj;

    pG->flags_54 |= 0x800;
#line 95 "D:/Bio4/Prog/r10b.cpp"
    r10b_work = (R10bWork*) MEM_CALLOC(sizeof(R10bWork), 1, 0xd);

    SceExec(0x12, (TaskFunc) R10b_chkWater, 0, 2, 2, 0);
    EatMgr.registEffInfo(2, (AtEffInfo*) &r10b_eff_info);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(4, 0x12, 0, (TaskFunc) r10b_GakeEvent, 0, 1);
    }
    readEvent(1, 0, 0);
    readEvent(2, 0, 0);
    readEvent(3, 0, 0);
    readEvent(4, 0, 0);
    EvtMgr.SetFunc("evt_r10bs00_func", (void*) Evt_R10BS00_Func);
    EvtMgr.SetFunc("evt_r10bs10_func", (void*) Evt_R10BS10_Func);
    EvtMgr.SetFunc("evt_r10bs20_func", (void*) Evt_R10BS20_Func);
    EvtMgr.SetFunc("evt_r10bs21_func", (void*) Evt_R10BS21_Func);
    EvtMgr.SetFunc("evt_r10bs22_func", (void*) Evt_R10BS22_Func);
    pos.x = 79072.0f;
    pos.y = -1300.0f;
    pos.z = 45680.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    obj = SetFloatIsland(ROOM_ARC_PTR(pG->pRoomArc, 0x1E), ROOM_ARC_PTR(pG->pRoomArc, 0x1F), &pos, &rot);
    r10b_work->island[0] = obj;
    if (obj != 0) {
        FSet(obj->scale.x, 1.5f);
        FSet(obj->scale.y, 1.5f);
        FSet(obj->scale.z, 1.5f);
        ((cObj1c*) obj)->setMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x20), ROOM_ARC_PTR(pG->pRoomArc, 0x21),
                                   ROOM_ARC_PTR(pG->pRoomArc, 0x22), ROOM_ARC_PTR(pG->pRoomArc, 0x23));
    }
    pos.x = 12708.0f;
    pos.y = -1300.0f;
    pos.z = 33719.0f;
    rot.x = 0.0f;
    rot.y = 3.1415927f;
    rot.z = 0.0f;
    obj = SetFloatIsland(ROOM_ARC_PTR(pG->pRoomArc, 0x1E), ROOM_ARC_PTR(pG->pRoomArc, 0x1F), &pos, &rot);
    r10b_work->island[1] = obj;
    if (obj != 0) {
        FSet(obj->scale.x, 2.0f);
        FSet(obj->scale.y, 2.0f);
        FSet(obj->scale.z, 2.0f);
        ((cObj1c*) obj)->setMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x20), ROOM_ARC_PTR(pG->pRoomArc, 0x21),
                                   ROOM_ARC_PTR(pG->pRoomArc, 0x22), ROOM_ARC_PTR(pG->pRoomArc, 0x23));
    }
    pos.x = 412.0f;
    pos.y = -1300.0f;
    pos.z = 64963.0f;
    rot.x = 0.0f;
    rot.y = 1.5707964f;
    rot.z = 0.0f;
    obj = SetFloatIsland(ROOM_ARC_PTR(pG->pRoomArc, 0x1E), ROOM_ARC_PTR(pG->pRoomArc, 0x1F), &pos, &rot);
    r10b_work->island[2] = obj;
    if (obj != 0) {
        FSet(obj->scale.x, 1.7f);
        FSet(obj->scale.y, 1.7f);
        FSet(obj->scale.z, 1.7f);
        ((cObj1c*) obj)->setMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x20), ROOM_ARC_PTR(pG->pRoomArc, 0x21),
                                   ROOM_ARC_PTR(pG->pRoomArc, 0x22), ROOM_ARC_PTR(pG->pRoomArc, 0x23));
    }
    pos.x = 55820.0f;
    pos.y = -1300.0f;
    pos.z = 81636.0f;
    rot.x = 0.0f;
    rot.y = 0.7853982f;
    rot.z = 0.0f;
    obj = SetFloatIsland(ROOM_ARC_PTR(pG->pRoomArc, 0x1E), ROOM_ARC_PTR(pG->pRoomArc, 0x1F), &pos, &rot);
    r10b_work->island[3] = obj;
    if (obj != 0) {
        FSet(obj->scale.x, 1.5f);
        FSet(obj->scale.y, 1.5f);
        FSet(obj->scale.z, 1.5f);
        ((cObj1c*) obj)->setMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x20), ROOM_ARC_PTR(pG->pRoomArc, 0x21),
                                   ROOM_ARC_PTR(pG->pRoomArc, 0x22), ROOM_ARC_PTR(pG->pRoomArc, 0x23));
    }
    EstSet(0, -1, 0, 0, 1, 2, 1, 2, (u32) zero, zero);
    EstSet((int) pPL, -1, 0, 0, 1, 5, 1, 3, (u32) zero, zero);
    r10b_work->boat = EmSetFromList2(0xA2, 1);
    RoomEfmRegist(SmdGetGroupObjPtr(0x58), 0x60);
    SetSstAddAreaFlag(0x800);
    r10b_setEm();
}

void R10bMain()
{
}

// Event files by readEvent index (defined here: the strings follow R10bInit's in .rodata).
static char* r10b_evtName[6] = {
    "evd/r10bs20.evd", "evd/r10bs21.evd", "evd/r10bs22.evd", "evd/r10bs00.evd", "evd/r10bs10.evd", 0,
};

// Loads event `no` (r10b_evtName) through a data unit; with `wait` the data is swapped into the
// boss module's block and `out` receives its address.
extern "C" int readEvent(int no, int wait, void** out)
{
    if (out != 0) {
        *out = 0;
    }
    if (R10B_WORK->evt[no] == 0) {
        R10B_WORK->evt[no] = DC.setData(EvtMgr.NameChange(r10b_evtName[no]));
        if (R10B_WORK->evt[no] == 0) {
            goto fail;
        }
    }
    if (wait != 0) {
        ReadModule* m;
        u32 max;

        if (R10B_WORK->evt[no]->waitLoadOk() == 0) {
            R10B_WORK->evt[no]->setCommand(3, 0, 0);
            pLog->err(0, 0, "readEvent() : out of memory (0x%x)", R10B_WORK->evt[no]->size);
            return 0;
        }
        EspEmDataSwapPush(0x2F);
        m = SearchEmModule(0x2F);
        max = m->size;
        if (R10B_WORK->evt[no]->size > max) {
            // `return 0` (not `goto fail`): at sched2 the block continues past the err call with
            // `li r3,0`, whose output dependence on the pLog load and the block-end jump rank the
            // `mr r7,size` and `lwz r3` above the string `lis`; jump2 then cross-jumps the tail.
            pLog->err(0, 0, "readEvent() : event size too large!![%d]>[%d]", R10B_WORK->evt[no]->size, max);
            return 0;
        }
        MemorySwap(m->pArc, (u32) R10B_WORK->evt[no]->addr, R10B_WORK->evt[no]->size);
        *out = m->pArc;
    } else {
        R10B_WORK->evt[no]->setCommand(2, 0, 0);
    }
    return 1;
fail:
    return 0;
}

extern "C" void freeEvent(int no)
{
    if (r10b_work->evt[no] != 0) {
        ReadModule* m;

        m = SearchEmModule(0x2F);
        MemorySwap(m->pArc, (u32) r10b_work->evt[no]->addr, r10b_work->evt[no]->size);
        EspEmDataSwapPop(0x2F);
        r10b_work->evt[no]->setCommand(3, 0, 0);
    }
}

// Waits for the boss's death: the death event, then the chapter end (or the escape event).
static void R10b_chkEmDie()
{
    void* evt;
    u32 key;
    u32 key2;
    cEm* boss;

    SceSleep(1);
    PSet(r10b_work->em0, GetEmPtrFromList(0xA0));
    boss = r10b_work->boss;
    Cckpt.life.flags = (u32) boss;
    for (;;) {
        if (DebugTrg(1)) {
            boss->hp = 1;
        }
        if (boss != 0 && boss->checkStatus(5) == 0) {
            SndRoomStrStop(0);
            SndEventStrStop(0);
            ((cEm2f*) GetEmPtrFromList(0xA0))->setDie();
            SceEventStart(0);
            BitOn(pG->flags_54, 0x400);
            EmMgr.destroy(r10b_work->boss);
            EffectEventDelete();
            DmgMgr.beginEvent(0);
            SceSleep(2);
            if (readEvent(0, 1, &evt)) {
                if (EvtMgr.SetEvt(evt, 0) == 0) {
                    r10b_work->count = 30;
                }
                r10b_waitEvt();
                freeEvent(0);
            }
            if (r10b_work->count > 14) {
                if (readEvent(2, 1, &evt)) {
                    if (EvtMgr.SetEvt(evt, &key) != 0) {
                        ((Event*) key)->status |= 0x400;
                    }
                    r10b_waitEvt();
                    freeEvent(2);
                }
            } else {
                if (readEvent(1, 1, &evt)) {
                    if (EvtMgr.SetEvt(evt, &key2) != 0) {
                        ((Event*) key2)->status |= 0x100000;
                        ((Event*) key2)->status |= 0x200;
                    }
                    r10b_waitEvt();
                    return;
                }
            }
            SceEventStart(0);
            SceSetChapterEnd(2, 6);
        }
        SceSleep(1);
    }
}

static inline void r10b_effDelete(int kind)
{
    EffectEspDelete(0, kind, 0, 0);
    EffectEspgenDelete(0, kind, 0);
    EffectEfmDelete(0, kind, 0);
}

// Watches the lake: the boss appears when the boat reaches the middle (event s10), the rain / spray
// effects follow the boat areas and the camera target effect.
static void R10b_chkWater()
{
    void* zero;

    for (;;) {
        if (!(pG->flags_174 & 0x40000000) && (pG->sceat_x17C & 0x20000000)) {
            void* evt;
            cEm* em;
            Vec pos;

            BitOn(pG->flags_174, 0x40000000);    // reference RMW: the r10b_work load waits for the store
            EmMgr.destroy(r10b_work->boat);
            EffectEventDelete();
            DmgMgr.beginEvent(0);
            SceSleep(2);
            if (readEvent(4, 1, &evt)) {
                EvtMgr.SetEvt(evt, 0);
                while (EvtMgr.IsAliveEvt(evtKey(&EvtMgr), 0, 0)) {
                    SceSleep(1);
                }
                freeEvent(4);
            }
            readEvent(0, 0, 0);
            GamePointBossReset();
            r10b_work->boss = EmSetFromList2(0xA1, 1);
            SceExec(0x12, (TaskFunc) R10b_chkEmDie, 0, 0, 2, 0);
            CamCtrl.Comeback(0);
            em = GetEmPtrFromList(0xA0);
            pos.x = 67713.0f;
            pos.y = em->pos.y;
            pos.z = -31975.0f;
            ((cEm2f*) em)->setCamPos(&pos, -1.12f);
            pPL->x12F = 0;
        }
        zero = 0;
        if (pG->flags_174 & 0x80000000) {
            if (pG->sceat_x17C & 0x40000000) {
                pG->flags_174 &= ~0x80000000;
                r10b_effDelete(2);
                r10b_effDelete(3);
                SceSleep(1);
                EstSet(0, -1, 0, 0, 1, 5, 1, 3, (u32) zero, zero);
                EstSet(0, -1, 0, 0, 1, 2, 1, 2, (u32) zero, zero);
            }
        } else if ((int) pG->sceat_x17C < 0) {
            pG->flags_174 |= 0x80000000;
            r10b_effDelete(2);
            r10b_effDelete(3);
            SceSleep(1);
            EstSet(0, -1, 0, 0, 1, 4, 1, 3, 0, 0);
            EstSet(0, -1, 0, 0, 1, 3, 1, 2, 0, 0);
        }
        if (pG->flags_5010 & 0x200000) {
            if (!(pG->flags_174 & 0x20000000)) {
                pG->flags_174 |= 0x20000000;
                r10b_effDelete(3);
            }
        } else if (pG->flags_174 & 0x20000000) {
            pG->flags_174 &= ~0x20000000;
            r10b_effDelete(3);
            EstSet(0, -1, 0, 0, 1, 5, 1, 3, 0, 0);
        }
        if (pG->flags_5010 & 0x200000) {
            Estgen45SetTargetCamera(1);
        } else {
            Estgen45SetTargetCamera(0);
        }
        SceSleep(1);
    }
}

// The cliff event (s00): the boat is replaced and the rain effects restart.
static void r10b_GakeEvent()
{
    void* evt;

    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtSetEnable(4, 0);
        RsfSet(G_ROOM_ID, 0);
        SceEventStart(0);
        BitOn(pG->flags_54, 0x400);
        r10b_effDelete(2);
        r10b_effDelete(3);
        SceSleep(1);
        EstSet(0, -1, 0, 0, 1, 2, 1, 2, 0, 0);
        EmMgr.destroy(r10b_work->boat);
        DmgMgr.beginEvent(0);
        EffectEventDelete();
        SceSleep(2);
        if (readEvent(3, 1, &evt)) {
            EvtMgr.SetEvt(evt, 0);
            while (EvtMgr.IsAliveEvt(&EvtMgr.x34, 0, 0)) {
                SceSleep(1);
            }
            freeEvent(3);
        }
        PSet(r10b_work->boat, EmSetFromList2(0xA3, 0));    // reference store: the pG load waits for it
        pG->flags_54 |= 0x400;
        SceSleep(4);
        SceEventEnd(0);
        pG->flags_54 |= 0x400;
        r10b_effDelete(2);
        r10b_effDelete(3);
        SceSleep(1);
        EstSet(0, -1, 0, 0, 1, 4, 1, 3, 0, 0);
        EstSet(0, -1, 0, 0, 1, 3, 1, 2, 0, 0);
        pG->flags_54 &= ~0x400;
    }
}

extern "C" void Evt_R10BS00_Func(Event* e)
{
    void* mod;

    switch (e->funcMode) {
    case 0:
        r10b_work->island[0]->setNoSuspend(1);
        r10b_work->island[2]->setNoSuspend(1);
        break;
    case 1:
        SetSstAddAreaFlag(0);
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                Evt_R10BSXX_Func_Pl0f(e);
                Evt_R10BSXX_Func_Em2f(e);
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag &= ~0x01000000;
                    ((cModel*) mod)->x12F = 1;
                }
            }
            break;
        case 1:
        case 3:
        case 7:
            if (e->frame == 0 && !(pG->flags_500C & 0x400)) {
                BitOn(pG->flags_500C, 0x400);
                PSet(r10b_work->bino, new (&r10b_work->binoObj) IdBinocular);
                r10b_work->bino->init(&pG->Cam, ROOM_ARC_PTR(pG->pRoomArc, 0x26), ROOM_ARC_PTR(pG->pRoomArc, 0x27));
                if (e->cut != 1) {
                    IdBinocularCutinI(r10b_work->bino, 0);
                }
                PSet(r10b_work->focus, &r10b_work->focusObj);
                r10b_work->focus->init(-1);
            }
            r10b_work->bino->move(&pG->Cam);
            break;
        default:
            if (e->frame == 0 && (pG->flags_500C & 0x400)) {
                BitOff(pG->flags_500C, 0x400);
                r10b_work->bino->quit(&pG->Cam);
                r10b_work->bino->~IdBinocular();
                r10b_work->focus->quit();
            }
            break;
        }
        break;
    case 2:
        if (pG->flags_500C & 0x400) {
            BitOff(pG->flags_500C, 0x400);
            r10b_work->bino->quit(&pG->Cam);
            r10b_work->bino->~IdBinocular();
        }
        SetSstAddAreaFlag(0x800);
        r10b_work->island[0]->setNoSuspend(0);
        r10b_work->island[1]->setNoSuspend(0);
        r10b_work->island[2]->setNoSuspend(0);
        r10b_work->island[3]->setNoSuspend(0);
        break;
    }
}

// The boss's tentacle heads (obj 0x16) on its parts 0x1D..0x22, each with its motion started at a
// different frame; mode 1 releases them.
extern "C" void em2fTentacleMove(cEm* em, Event* e, int mode)
{
    u32 i;

    if (mode == 0) {
        void* bin;
        void* tpl;
        void* fcv;
        u16 step;

        if ((pG->flags_60 & 0x02000000) && (pG->flags_174 & 0x8000)) {
            return;
        }
        pG->flags_174 |= 0x8000;
        if (EvtMgr.GetBin(&bin, "obj/objmodel/obm3100.bin", 0) == 0) {
            return;
        }
        if (EvtMgr.GetBin(&tpl, "obj/objmodel/obm3100.tpl", 0) == 0) {
            return;
        }
        if (EvtMgr.GetBin(&fcv, "obj/objmodel/obm31000.fcv", 0) == 0) {
            return;
        }
        step = (*(u16*) fcv & 0x3FFF) / 6;
        for (i = 0; i < 6; i++) {
            if (R10B_WORK->head[i] == 0) {
                int parts;
                Vec pos;
                Vec rot;

                switch (i) {
                case 0:
                default:
                    parts = 0x1D;
                    break;
                case 1:
                    parts = 0x1E;
                    break;
                case 2:
                    parts = 0x1F;
                    break;
                case 3:
                    parts = 0x20;
                    break;
                case 4:
                    parts = 0x21;
                    break;
                case 5:
                    parts = 0x22;
                    break;
                }
                pos.x = 0.0f;
                pos.y = 0.0f;
                pos.z = 0.0f;
                rot.x = 1.5707964f;
                rot.y = 0.0f;
                rot.z = fRand1_1() * 3.1415927f;
                R10B_WORK->head[i] = SetObj16(bin, tpl, em, em, parts, 0xA, &pos, &rot);
                if (R10B_WORK->head[i] != 0) {
                    MotSetObj16(R10B_WORK->head[i], fcv, 4, i * step);
                }
            }
        }
    } else {
        for (i = 0; i < 6; i++) {
            if (r10b_work->head[i] != 0) {
                ((cObj16*) r10b_work->head[i])->clearLostWait();
                R10B_WORK->head[i] = 0;
            }
        }
    }
}

extern "C" void Evt_R10BS10_Func(Event* e)
{
    void* mod;

    switch (e->funcMode) {
    case 0:
        if (pG->flags_60 & 0x02000000) {
            r10b_effDelete(2);
            r10b_effDelete(3);
            EstSet(0, -1, 0, 0, 1, 3, 1, 2, 0, 0);
        }
        {
            // A user variable: jump.c's thread_jumps never equivalences a REG_USERVAR_P pseudo, so
            // the re-test survives (the original re-reads the flag after the effect calls).
            u32 f = pG->flags_60;

            if (f & 0x02000000) {
                pG->flags_174 &= ~0x8000;
            }
        }
        break;
    case 1:
        SetSstAddAreaFlag(0);
        if (e->cut == 3 && e->frame == 0) {
            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                cLight* l = LightMgr.getKindLight(1);

                if (l != 0) {
                    l->setParent((cModel*) mod);
                }
            }
        }
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                void* mod0;

                Evt_R10BSXX_Func_Pl0f(e);
                Evt_R10BSXX_Func_Em2f(e);
                if (e->GetMod(&mod0, "obm1300", 0, 0) == 1) {
                    ((cModel*) mod0)->lightInfo.x50 = 8;
                }
                if (e->GetMod(&mod0, "pl0000", 0, 0) == 1) {
                    ((cModel*) mod0)->be_flag |= 0x100000;
                }
            }
            break;
        case 4:
            if (e->frame == 0) {
                void* em;

                if (e->GetMod(&em, "em2f00", 0, 0) == 1) {
                    em2fTentacleMove((cEm*) em, e, 0);
                }
            }
            if (e->frame == 40) {
                void* em;

                if (e->GetMod(&em, "em2f00", 0, 0) == 1) {
                    em2fTentacleMove((cEm*) em, e, 1);
                }
            }
            break;
        case 3:
        case 5:
            if (e->frame == 0) {
                void* em;

                if (e->GetMod(&em, "em2f00", 0, 0) == 1) {
                    em2fTentacleMove((cEm*) em, e, 1);
                }
            }
            break;
        }
        break;
    case 2:
        SetSstAddAreaFlag(0x800);
        if (pG->flags_60 & 0x02000000) {
            r10b_effDelete(2);
            r10b_effDelete(3);
        }
        break;
    case 3:
        SndRoomStrStart(1, 0, 1);
        break;
    }
}

extern "C" void Evt_R10BS20_Func(Event* e)
{
    void* mod;

    switch (e->funcMode) {
    case 0:
        if (pG->flags_60 & 0x02000000) {
            r10b_effDelete(2);
            r10b_effDelete(3);
            EstSet(0, -1, 0, 0, 1, 2, 1, 2, 0, 0);
        }
        EvtFlgOnStatus(e, 3);
        e->cancelCut = 9;
        break;
    case 1:
        SetSstAddAreaFlag(0);
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                Evt_R10BSXX_Func_Pl0f(e);
                Evt_R10BSXX_Func_Em2f(e);
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 0);
                    ModelInfoSetTrans((cModel*) mod, 6, 0);
                }
            }
            break;
        case 9:
            if (e->frame == 100) {
                e->BeginActBtn(0x29);
            }
            break;
        }
        break;
    case 2:
        e->EndActBtn();
        r10b_work->count = e->GetActBtnCount();
        SetSstAddAreaFlag(0x800);
        if (pG->flags_60 & 0x02000000) {
            r10b_effDelete(2);
            r10b_effDelete(3);
        }
        break;
    case 3:
        EvtMgr.EvtSndStrPlay(evtKey(&EvtMgr), 1, 0x1D, 1, 0.0f);
        break;
    }
}

extern "C" void Evt_R10BS21_Func(Event* e)
{
    void* mod;

    switch (e->funcMode) {
    case 0:
        if (pG->flags_60 & 0x02000000) {
            r10b_effDelete(2);
            r10b_effDelete(3);
            EstSet(0, -1, 0, 0, 1, 2, 1, 2, 0, 0);
        }
        break;
    case 1:
        SetSstAddAreaFlag(0);
        if (e->cut == 0 && e->frame == 0) {
            Evt_R10BSXX_Func_Pl0f(e);
            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 2, 0);
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
        }
        break;
    case 2:
        SetSstAddAreaFlag(0x800);
        if (pG->flags_60 & 0x02000000) {
            r10b_effDelete(2);
            r10b_effDelete(3);
        }
        break;
    }
}

extern "C" void Evt_R10BS22_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        if (pG->flags_60 & 0x02000000) {
            r10b_effDelete(2);
            r10b_effDelete(3);
            EstSet(0, -1, 0, 0, 1, 2, 1, 2, 0, 0);
        }
        SmdSetTrans(0x59, 0);
        break;
    case 1:
        SetSstAddAreaFlag(0);
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                void* mod;

                Evt_R10BSXX_Func_Pl0f(e);
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 0);
                    ModelInfoSetTrans((cModel*) mod, 6, 0);
                }
            }
            break;
        case 1:
            if (e->frame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 1);
                    ModelInfoSetTrans((cModel*) mod, 6, 1);
                }
            }
            break;
        case 2:
            break;
        }
        break;
    case 2:
        SetSstAddAreaFlag(0x800);
        if (pG->flags_60 & 0x02000000) {
            r10b_effDelete(2);
            r10b_effDelete(3);
        }
        SmdSetTrans(0x59, 1);
        break;
    }
}

// The player model of the boat events: no shadow, the parts 3 (the harpoon) hidden.
extern "C" void Evt_R10BSXX_Func_Pl0f(Event* e)
{
    void* mod;

    if (e->GetMod(&mod, "pl0f00", 0, 0) == 1) {
        cModel* p;

        ((cModel*) mod)->be_flag |= 0x80;
        ((cModel*) mod)->lightInfo.x50 = 4;
        p = ((cModel*) mod)->getPartsPtr(3);
        p->scale.x = 0.0f;
        p->scale.y = 0.0f;
        p->scale.z = 0.0f;
    }
}

extern "C" void Evt_R10BSXX_Func_Em2f(Event* e)
{
    void* mod;

    e->GetMod(&mod, "em2f00", 0, 0);
}

// The five fish (enemy 0x27) of the lake.
static void r10b_setEm()
{
    EmListData d;

    d.rot[0] = 0;
    d.rot[2] = 0;
    d.id = 0x27;
    d.pos[0] = -0xFE4;
    d.pos[1] = -0x82;
    d.pos[2] = 0x81C;
    d.rot[1] = -0xBBB;
    d.type = 0;
    d.x3 = 0;
    d.flags4 = 0;
    d.xB = 0;
    d.hp = 1000;
    d.x1A = 0;
    EmSetEvent(&d);

    d.id = 0x27;
    d.pos[0] = -0x1090;
    d.pos[1] = -0x82;
    d.pos[2] = 0xA14;
    d.rot[1] = -0x19F4;
    d.type = 0;
    d.x3 = 0;
    d.flags4 = 0;
    d.xB = 0;
    d.hp = 1000;
    d.x1A = 0;
    EmSetEvent(&d);

    d.id = 0x27;
    d.pos[0] = -0x10D8;
    d.pos[1] = -0x82;
    d.pos[2] = 0x766;
    d.rot[1] = -0x19F4;
    d.type = 0;
    d.x3 = 0;
    d.flags4 = 0;
    d.xB = 0;
    d.hp = 1000;
    d.x1A = 0;
    EmSetEvent(&d);

    d.id = 0x27;
    d.pos[0] = -0x105E;
    d.pos[1] = -0x82;
    d.pos[2] = 0x8F5;
    d.rot[1] = -0xBBB;
    d.type = 0;
    d.x3 = 0;
    d.flags4 = 0;
    d.xB = 0;
    d.hp = 1000;
    d.x1A = 0;
    EmSetEvent(&d);

    d.id = 0x27;
    d.pos[0] = -0xF6D;
    d.pos[1] = -0x82;
    d.pos[2] = 0x93A;
    d.rot[1] = -0xBBB;
    d.type = 0;
    d.x3 = 0;
    d.flags4 = 0;
    d.xB = 0;
    d.hp = 1000;
    d.x1A = 0;
    EmSetEvent(&d);
}

// the split object's .data is 8-aligned
asm(".section .data; .balign 8");
