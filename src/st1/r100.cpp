#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
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
#include "emdoor.h"
#include "emwindow.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "read.h"
#include "dvd.h"
#include "datactrl.h"
#include "player.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "fade.h"
#include "pad.h"
#include "math_sub.h"
#include "motion.h"
#include "TexRender.h"
#include "rnd.h"
#include "room_data.h"
#include "db_log.h"

// Room 1-00 (D:/Bio4/Prog/r100.cpp): the village approach; the police car, the two officers
// (s03: the first Ganado kills one, s20: the truck runs the car off the bridge, s40: the ravine).

void Obj18CmfOn(cObj* o, u32 n);        // game/obj18.cpp
extern "C" void EventCarInit(Event* e);  // st1_0/r120.cpp

// The original passes an uninitialised int to cEmDoor::setCloseLock(int) (no r4 setup).
void cEmDoorSetCloseLock(cEm* door) asm("setCloseLock__7cEmDoori");

struct R100Work {
    TexRenderMng* tex;    // 0x00  the pond render target (setTexRender)
    cEm* em;              // 0x04  the s03 Ganado (enemy list entry set up by hand)
    cEm* ems[7];          // 0x08  the ambush after s03 (r100_em_set)
    cEm* emHouse;         // 0x24  the Ganado of the house event
    cObj* car;            // 0x28  the police car
    cObj* carSub;         // 0x2C  the car's second model
    cObj* cop[2];         // 0x30  the two officers in the car (motion models)
    cObj* smd;            // 0x38  the region-specific object
    u8 pad_3C[4];
    cDataUnit* evt[10];   // 0x40  evd/r100sXX.evd data units (r100_evtName)
    u8 pad_68[0x80 - 0x68];
    u32 cnt;              // 0x80  frames until the officers go back to their idle motion
    u32 se;               // 0x84  SndCall handle of the officers' line
};

// One-member struct: every store into the work reloads the pointer afterwards.
struct R100WorkPtr {
    R100Work* p;
};
static u8 r100_texTbl[0x20];
static R100WorkPtr r100_work;
#define W r100_work.p

// Hit effects of attribute types 2, 4 and 5
static const AtEffInfo r100_eff_info2 = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};
static const AtEffInfo r100_eff_info4 = {
    0, {0xD2, 0}, {1, 0xF}, {0, 0xB}, {0, 0xC}, {1, 0xE}, {1, 0xE}, {0, 0x36}, {0, 0x27},
};
static const AtEffInfo r100_eff_info5 = {
    2, {0xD2, 0}, {0xD2, 0}, {0xD2, 0}, {0xD2, 0}, {0xD2, 0}, {0xD2, 0}, {0xD2, 0}, {0xD2, 0},
};

extern "C" int readEvent(int no, int wait, void** out);
extern "C" void freeEvent(int no, int swap);
extern "C" void r100_em_set();
static void r100_GakeEvent(int arg);
static void r100_StartEvent();
static void r100_DoorCk();
static void r100_WindowBreakCk();
static void r100_HouseEvent_exit();
static void r100_HouseEvent();
static void r100_StreanChk();
static void r100_Sce_look();
static void r100_Sce_zombi_dead(cEm* em);
extern "C" void r100_Car_pos_move();
extern "C" void r100_trap_set();
static void r100_MesDoor();
static void r100_MesTruck();
static void r100_MesGanado();
static void r100_MesCar00();
static void r100_MesCar01();
static void r100_MesBrige();
static void r100_EventBrige();
extern "C" void setTexRender();
extern "C" void Evt_R100S40_Func(Event* e);
extern "C" void Evt_R100S20_Func(Event* e);
extern "C" void Evt_R100S03_Func(Event* e);
static void r100_mes_gaikotu_bgm();
static void r100_mes_gaikotu();
static void r100_mes_gaikotu_bgm_down();
static void r100_mes_gaikotu_bgm_up();

// em.h's cEm carries the player fields (EmMgr's stride 0xDE0); the class the original puts on the
// stack here is 0x3E0 (two of them make the 0x818 frame). HEADER DEBT: make these `cEm em;` once
// em.h splits the work area off (r103 has the same note).
class R100Em : public cModel {
public:
    u8 pad_320[0x378 - 0x320];
    PlArc* subArc;        // 0x378
    u8 pad_37C[0x3E0 - 0x37C];

    R100Em() asm("__3cEm");
};

void R100Init()
{
    cObj* o;
    // The second cop is a separate local: reusing `o` gives its `addi r4, o, 0x1d8` an extra
    // (anti-)dependent, the later `o = SetObjSmd()`, and the scheduler then issues it before the
    // argument `li`s (the original has it last).
    cObj* o2;
    cModelInfo* info;
    u32 flag;

    BitOff(pG->flags_54, 0x400);
    if (pG->x4F9F == 1 || DebugTrg(1)) {
        RsfSet(G_ROOM_ID, 10);
        RsfSet(G_ROOM_ID, 3);
        RsfSet(G_ROOM_ID, 13);
    }
#line 207 "D:/Bio4/Prog/r100.cpp"
    W = (R100Work*) MEM_CALLOC(sizeof(R100Work), 1, 0xd);
    EmReadSearch(0x12, 0, 0);
    if (pSys->region == 0) {
        SetSstDispFlag(0x12, 0);
        SceAtSetEnable(0x17, 0);
    } else {
        Vec pos = {45442.0f, -430.0f, -8800.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};
        ReadModule* m = SearchEmModule(0x12);

        if (m) {
            R100Em em;
            R100Em* pe = &em;

            em.subArc = (PlArc*) m->pArc;
            W->smd = SetObjSmd(ROOM_ARC_PTR(pG->pRoomArc, 0x31), ROOM_ARC_PTR(pG->pRoomArc, 0x32), &pos, &rot, 0x10, 1);
            BitOn(W->smd->be_flag, 0x1000);
            SetObjSmd(ROOM_ARC_PTR(pG->pRoomArc, 0x33), PL_ARC_PTR(pe->subArc, 0x255), &pos, &rot, 0x10, 1)->be_flag |= 0x1000;
            SetSstDispFlag(0x12, 1);
        }
    }
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        readEvent(5, 0, 0);
        readEvent(7, 0, 0);
        readEvent(8, 0, 0);
        readEvent(9, 0, 0);
    }
    SmdGetObjPtr(0x37)->be_flag &= ~2;
    SmdGetObjPtr(0x38)->be_flag &= ~2;
    SmdGetObjPtr(0x23)->be_flag |= 2;
    SmdGetObjPtr(0x44)->be_flag |= 2;
    setTexRender();
    SceAtSetEnable(1, 0);
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        EmListData d;
        cEm* em;

        d.id = 0x12;
        d.type = 0;
        d.x3 = 0x13;
        d.flags4 = 0x21000020;
        d.pos[0] = -7763;
        d.pos[1] = 86;
        d.pos[2] = -3650;
        d.rot[0] = 0;
        d.rot[1] = 0x2000;
        d.rot[2] = 0;
        d.hp = 500;
        d.x1A = 10;
        d.xB = 0;
        em = EmSetEvent(&d);
        W->em = em;
        if (em == 0 || em == errEm) {
            pLog->err(0, 0, "R100Init : set failed");
        }
        {
            R100Em em;
            R100Em* pe = &em;

            // The original's store is a scalar one through the pointer (the pG load after it is not
            // hoisted above it, and the address stays `0x378(pe)`): a member store is a struct store
            // and a reference setter folds the address into the frame.
            *(PlArc**) ((u8*) pe + 0x378) = (PlArc*) EmReadSearch(0x12, 0, 0);
            EvtMgr.SetBin("em/pl07/pl0700a.bin", ROOM_ARC_PTR(pG->pRoomArc, 0x26), 0, 2);
            EvtMgr.SetBin("em/pl07/pl0700.bin", ROOM_ARC_PTR(pG->pRoomArc, 0x24), 0, 2);
            EvtMgr.SetBin("em/pl07/pl0700.tpl", ROOM_ARC_PTR(pG->pRoomArc, 0x25), 0, 2);
            EvtMgr.SetBin("obj/objmodel/obm2a00.bin", ROOM_ARC_PTR(pG->pRoomArc, 0x1D), 0, 2);
            EvtMgr.SetBin("obj/objmodel/obm2a00.tpl", ROOM_ARC_PTR(pG->pRoomArc, 0x1E), 0, 2);
        }
    }
    EvtMgr.SetFunc("evt_r100s03_func", (void*) Evt_R100S03_Func);
    EvtMgr.SetFunc("evt_r100s20_func", (void*) Evt_R100S20_Func);
    EvtMgr.SetFunc("evt_r100s40_func", (void*) Evt_R100S40_Func);
    {
        Vec pos;
        Vec rot;

        pos.x = -102497.0f;
        pos.y = -687.0f;
        pos.z = 1953.0f;
        rot.x = 0.0f;
        rot.y = 0.812328577f;
        rot.z = 0.0f;
        o = SetObjSmd(ROOM_ARC_PTR(pG->pRoomArc, 0x1D), ROOM_ARC_PTR(pG->pRoomArc, 0x1E), &pos, &rot, 0x10, 1);
        W->car = o;
        if (o == 0) {
            pLog->err(0, 0, "R100Init : set failed");
            return;
        }
        W->car->be_flag |= 0x10;
        W->car->pPartsHead[10].rot.x = 0.436332315f;
        W->car->pPartsHead[11].scale.x = 0.0f;
        W->car->pPartsHead[11].scale.y = 0.0f;
        W->car->pPartsHead[11].scale.z = 0.0f;
        W->car->pPartsHead[13].scale.x = 0.0f;
        W->car->pPartsHead[13].scale.y = 0.0f;
        W->car->pPartsHead[13].scale.z = 0.0f;
        W->car->pPartsHead[15].scale.x = 0.0f;
        W->car->pPartsHead[15].scale.y = 0.0f;
        W->car->pPartsHead[15].scale.z = 0.0f;
        W->car->partsMatCalc();
        o->setNoSuspend(0);
    }
    {
        Vec pos;
        Vec rot;

        pos.x = -60821.0f;
        pos.y = -55.0f;
        pos.z = -17230.0f;
        rot.x = 0.0f;
        rot.y = -1.50098312f;
        rot.z = 0.0f;
        o = SetObjSmd(ROOM_ARC_PTR(pG->pRoomArc, 0x1F), ROOM_ARC_PTR(pG->pRoomArc, 0x20), &pos, &rot, 0x10, 1);
        W->carSub = o;
        if (o == 0) {
            pLog->err(0, 0, "R100Init : set failed");
            return;
        }
        W->carSub->be_flag |= 0x10;
        o->setNoSuspend(0);
    }
    if (RsfCheck(G_ROOM_ID, 10)) {
        r100_Car_pos_move();
        r100_trap_set();
        SceAtSetEnable(1, 1);
        SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) r100_GakeEvent, 0, 1);
        readEvent(4, 0, 0);
    } else {
        Vec pos;
        Vec rot;

        SetSstDispFlag(0x11, 0);
        pos.x = -102497.0f;
        pos.y = -687.0f;
        pos.z = 1953.0f;
        rot.x = 0.0f;
        rot.y = 0.812328577f;
        rot.z = 0.0f;
        o = SetObjSmd(ROOM_ARC_PTR(pG->pRoomArc, 0x24), ROOM_ARC_PTR(pG->pRoomArc, 0x25), &pos, &rot, 0x10, 1);
        W->cop[0] = o;
        if (o == 0) {
            pLog->err(0, 0, "R100Init : set failed");
            return;
        }
        info = ModInfoMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x27), ROOM_ARC_PTR(pG->pRoomArc, 0x28));
        if (info) {
            o->addModel(info);
        }
        W->cop[0]->x12F = 4;
        MotionSetCore(o, &o->mot, ROOM_ARC_PTR(pG->pRoomArc, 0x2B), 0, 0, 5, 0);
        o->setNoSuspend(0);
        o2 = SetObjSmd(ROOM_ARC_PTR(pG->pRoomArc, 0x26), ROOM_ARC_PTR(pG->pRoomArc, 0x25), &pos, &rot, 0x10, 1);
        W->cop[1] = o2;
        if (o2 == 0) {
            pLog->err(0, 0, "R100Init : set failed");
            return;
        }
        info = ModInfoMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x29), ROOM_ARC_PTR(pG->pRoomArc, 0x2A));
        if (info) {
            o2->addModel(info);
        }
        W->cop[1]->x12F = 4;
        MotionSetCore(o2, &o2->mot, ROOM_ARC_PTR(pG->pRoomArc, 0x2C), 0, 0, 5, 0);
        o2->setNoSuspend(0);
    }
    SceAtSetEnable(0xA, 0);
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        if (W->em != 0 && W->em != errEm) {
            SceExecLinkEmDead(W->em, 0x12, (TaskFunc) r100_Sce_zombi_dead, (int) W->em, 0);
        }
    } else {
        r100_em_set();
    }
    flag = RsfCheck(G_ROOM_ID, 3);
    if (flag == 0) {
        SceAtSetEnable(0xA, 1);
        SceAtDataSet_exec(0xA, 0x12, 0, (TaskFunc) r100_Sce_look, 0, 2);
    } else {
        cEm* em = W->em;

        if (em != 0 && em != errEm) {
            em->xFC = 1;
            em->xFD = 0x10;
            em->xFE = 0;
            em->xFF = 0;
            em->flags_3C8 |= 1;
        }
    }
    SceExec(0x12, (TaskFunc) r100_WindowBreakCk, 0, 0, 2, 0);
    SceExec(0x12, (TaskFunc) r100_DoorCk, 0, 0, 2, 0);
    SceExec(0x12, (TaskFunc) r100_StreanChk, 0, 0, 2, 0);
    SceAtDataSet_exec(0x15, 0x12, 0, (TaskFunc) r100_HouseEvent, 0, 1);
    SceAtDataSet_exec(0xC, 0x12, 0, (TaskFunc) r100_MesTruck, 0, 1);
    SceAtDataSet_exec(0x19, 0x12, 0, (TaskFunc) r100_MesCar00, 0, 1);
    SceAtDataSet_exec(0x1A, 0x12, 0, (TaskFunc) r100_MesCar01, 0, 1);
    SceAtDataSet_exec(0xB, 0x12, 0, (TaskFunc) r100_MesDoor, 0, 1);
    SceAtSetEnable(0xB, 0);
    SceAtDataSet_exec(0x1B, 0x12, 0, (TaskFunc) r100_EventBrige, 0, 1);
    if (RsfCheck(G_ROOM_ID, 10)) {
        SceAtSetEnable(0xC, 0);
        SceAtSetEnable(0x19, 0);
        SceAtSetEnable(0x1A, 0);
        SceAtDataSet_exec(0x18, 0x12, 0, (TaskFunc) r100_MesBrige, 0, 1);
        SceAtSetEnable(0x1C, 1);
        SceAtSetEnable(0x1E, 0);
    } else {
        SceAtSetEnable(0x1C, 0);
        SceAtSetEnable(0x1E, 1);
    }
    if (RsfCheck(G_ROOM_ID, 13) == 0) {
        RsfSet(G_ROOM_ID, 13);
        SeAtSetOnOff(2, 0);
        SceExec(0x12, (TaskFunc) r100_StartEvent, 0, 0, 2, 0);
    }
    EatMgr.registEffInfo(2, (AtEffInfo*) &r100_eff_info2);
    EatMgr.registEffInfo(4, (AtEffInfo*) &r100_eff_info4);
    EatMgr.registEffInfo(5, (AtEffInfo*) &r100_eff_info5);
    SceAtDataSet_exec(0x16, 0x12, 0, (TaskFunc) r100_mes_gaikotu, 0, 1);
    SceAtDataSet_exec(0x22, 0x12, 0, (TaskFunc) r100_mes_gaikotu_bgm_down, 0, 1);
    SceAtDataSet_exec(0x23, 0x12, 0, (TaskFunc) r100_mes_gaikotu_bgm_up, 0, 1);
}

static inline f32 FCRef(const f32& v) { return v; }

void R100Main()
{
    static const f32 vol = 0.0f;

    if (RsfCheck(G_ROOM_ID, 0) == 0 && SceAtHitCheck(6)) {
        RsfSet(G_ROOM_ID, 0);
        readEvent(0, 0, 0);
        readEvent(3, 0, 0);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        if (SceAtHitCheck(7) || SceAtHitCheck(8)) {
            RsfSet(G_ROOM_ID, 1);
        }
    }
    if (RsfCheck(G_ROOM_ID, 12) == 0 && RsfCheck(G_ROOM_ID, 3) && !SceAtHitCheck(0xD) &&
        !(pG->flags_500C & 0x1000)) {
        RsfSet(G_ROOM_ID, 12);
        SndStrReq(1, 4, 4, 400, 0, FCRef(vol));
        SndStrReq(1, 5, 4, 400, 0, FCRef(vol));
        SndStrReq(1, 6, 4, 400, 0, FCRef(vol));
    }
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        if (W->cnt != 0) {
            W->cnt--;
            if (W->cnt == 0) {
                MotionSetCore(W->cop[0], &W->cop[0]->mot, ROOM_ARC_PTR(pG->pRoomArc, 0x2B), 0, 10, 5, 0);
                MotionSetCore(W->cop[1], &W->cop[1]->mot, ROOM_ARC_PTR(pG->pRoomArc, 0x2C), 0, 10, 5, 0);
            }
        } else if ((int) pG->flags_5014 < 0 && (Key.trg & 0x80) && W->cop[0] && W->cop[1]) {
            if (pG->sceat_x17C & 0x40000000) {
                MotionSetCore(W->cop[0], &W->cop[0]->mot, ROOM_ARC_PTR(pG->pRoomArc, 0x35), 0, 10, 5, 0);
                W->se = SndCall(6, 6, &W->cop[0]->pos, 0, 0, 0);
            } else {
                MotionSetCore(W->cop[1], &W->cop[1]->mot, ROOM_ARC_PTR(pG->pRoomArc, 0x36), 0, 10, 5, 0);
                W->se = SndCall(6, 5, &W->cop[0]->pos, 0, 0, 0);
            }
            W->cnt = (u32) MotionGetMaxFrame(&W->cop[0]->mot);
            {
                int ls = cMes.getWork()->lineSpace;
                int fh = cMes.getWork()->fontH;

                cMes.MesSet(0x33, 0x64, 0x147 - fh - ls, 0x52, 0, 0, 4);
            }
        }
    }
}

// The original's .data is 8-aligned (r105 has the same).
asm(".section .data; .balign 8");
static char* r100_evtName[10] = {
    "evd/r100s03.evd", "evd/r100s01.evd", "evd/r100s02.evd", "evd/r100s20.evd", "evd/r100s30.evd",
    "evd/r100s41.evd", "evd/r100s42.evd", "evd/r100s43.evd", "evd/r100s44.evd", "evd/r100s40.evd",
};

// Loads event `no` (r100_evtName) through a data unit; with `wait` the data is swapped into the
// Ganado module's block (events 0, 4, 9) or loaded in place, and `out` receives its address.
extern "C" int readEvent(int no, int wait, void** out)
{
    if (out != 0) {
        *out = 0;
    }
    if (W->evt[no] == 0) {
        W->evt[no] = DC.setData(EvtMgr.NameChange(r100_evtName[no]));
        if (W->evt[no] == 0) {
            goto fail;
        }
    }
    if (wait != 0) {
        if (no == 0 || no == 4 || no == 9) {
            ReadModule* m;

            EspEmDataSwapPush(0x12);
            m = SearchEmModule(0x12);
            if (W->evt[no]->size > m->size) {
                pLog->err(0, 0, "readEvent() : event size too large!![%d]>[%d]", W->evt[no]->size, m->size);
                goto fail;
            }
            if (W->evt[no]->waitLoadOk() == 0) {
                W->evt[no]->setCommand(3, 0, 0);
                pLog->err(0, 0, "r100::readEvent() : out of memory");
                goto fail;
            }
            MemorySwap(m->pArc, (u32) W->evt[no]->addr, W->evt[no]->size);
            {
                void* arc = m->pArc;

                if (out != 0) {
                    *out = arc;
                }
            }
        } else {
            W->evt[no]->setCommand(1, 0, 1);
            if (W->evt[no]->waitUseOk() == 0) {
                W->evt[no]->setCommand(3, 0, 0);
                pLog->err(0, 0, "readEvent() : out of memory.", no, r100_evtName[no]);
                pLog->err(0, 0, "readEvent() : size(0x%x)[%d:%s]", W->evt[no]->size, no, r100_evtName[no]);
                return 0;
            }
            {
                void* addr = W->evt[no]->addr;

                if (out != 0) {
                    *out = addr;
                }
            }
        }
    } else {
        if (no == 9) {
            W->evt[no]->setCommand(2, 0, 1);
        } else {
            W->evt[no]->setCommand(2, 0, 0);
        }
    }
    return 1;
fail:
    return 0;
}

extern "C" void freeEvent(int no, int swap)
{
    if (W->evt[no] != 0) {
        if (swap != 0 && (no == 0 || no == 4 || no == 9)) {
            ReadModule* m;

            m = SearchEmModule(0x12);
            MemorySwap(m->pArc, (u32) W->evt[no]->addr, W->evt[no]->size);
            EspEmDataSwapPop(0x12);
        }
        W->evt[no]->setCommand(3, 0, 0);
    }
}

// The ambush after the officer's death.
extern "C" void r100_em_set()
{
    W->ems[0] = EmSetFromList2(3, 1);
    W->ems[1] = EmSetFromList2(4, 1);
    W->ems[2] = EmSetFromList2(5, 1);
    W->ems[3] = EmSetFromList2(0x12, 1);
    W->ems[4] = EmSetFromList2(0x13, 1);
    W->ems[5] = EmSetFromList2(0x26, 1);
    W->ems[6] = EmSetFromList2(0x27, 1);
}

// Looking down the ravine at the car: the s40 event once, then a camera cut with a message.
static void r100_GakeEvent(int arg)
{
    Vec pos;
    void* evt;

    if (RsfCheck(G_ROOM_ID, 14)) {
        W->car->setNoSuspend(1);
        W->carSub->setNoSuspend(1);
        SceEventStart(0);
        CamCtrl.CutCall(9);
        SceMesSet(0x28, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.Comeback(0);
        SceEventEnd(0);
    } else {
        RsfSet(G_ROOM_ID, 14);
        pos = pPL->pos;
        SceEventStart(0);
        if (SndStrStatusCk(1, 0xA, 0x10)) {
            SndStrReq(1, 0xA, 8, 0, 0, 0.0f);
            while (SndStrStatusCk(1, 0xA, 0x10)) {
                SceSleep(1);
            }
        }
        SndRoomStrStop(3);
        W->car->setNoSuspend(1);
        W->carSub->setNoSuspend(1);
        if (readEvent(4, 1, &evt)) {
            EvtMgr.SetEvt(evt, (u32*) 0);
            while (EvtMgr.IsAliveEvt(&EvtMgr.x34, 0, 0)) {
                SceSleep(1);
            }
            freeEvent(4, 1);
        }
        SceEventEnd(0);
        pPL->setPos(&pos);
    }
}

// The room entry: the s01 event (skipped by the flag / route), then the player at the gate.
static void r100_StartEvent()
{
    int skip = 0;
    void* evt;
    u32 flag;

    SceEventStart(0);
    pG->flags_54 |= 0x400;
    SceSleep(1);
    BitOff(SmdGetObjPtr(0x44)->be_flag, 2);
    BitOn(pG->flags_5010, 0x800);
    BitOn(pG->flags_58, 0x20000);
    flag = pG->flags_54;
    if (flag & 0x40) {
        skip = 1;
    }
    if (!(pG->flags_54 & 0x40) && !(pG->flags_51C0 & 0x10)) {
        if (readEvent(9, 1, &evt)) {
            EvtMgr.SetEvt(evt, (u32*) 0);
            SceSleep(1);
            FadeSetW(0x80000002, 30, 0, 0);
            while (EvtMgr.IsAliveEvt(&EvtMgr.x34, 0, 0)) {
                SceSleep(1);
            }
            freeEvent(9, 1);
        }
    } else {
        pG->flags_54 &= ~0x400;
        freeEvent(9, 0);
    }
    BitOn(SmdGetObjPtr(0x44)->be_flag, 2);
    BitOff(pG->flags_5010, 0x800);
    {
        Vec pos;
        Vec* pp = &pos;

        pos.x = -99685.0f;
        pp->y = -484.0f;
        pp->z = -1343.0f;
        pPL->setPos(pp);
    }
    {
        Vec ang;
        cPlayer* p = pPL;

        ang.x = 0.0f;
        ang.y = 2.246f;
        ang.z = 0.0f;
        p->setAng(&ang);
    }
    SceEventEnd(0);
    BitOff(pG->flags_58, 0x20000);
    if (skip == 0 && !(pG->flags_51C0 & 0x10)) {
        OpeSetOpenTerm(0, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    OpeSetMdtNo(0);
    SeAtSetOnOff(2, 1);
    SndBgmTblSet(0x100, 1);
    SndRoomBgmStart(0, 0);
    SndRoomBgmStart(1, 0);
}

// The village gate door: locked open until the officer is dead, then locked shut while the
// ambush hunts the player, then a normal door.
static void r100_DoorCk()
{
    cEm* door;
    int found = 0;
    int i;

    if (getRoomEtcDoor(2, &door, 1) == 0) {
        return;
    }
    while (RsfCheck(G_ROOM_ID, 3) == 0) {
        ((cEmDoor*) door)->setOpenLock(0);
        SceSleep(1);
    }
    pG->flags_174 &= ~0x80000000;
    found = 0;
    while (RsfCheck(G_ROOM_ID, 4) == 0) {
        if ((int) pG->flags_174 < 0) {
            found = 1;
        }
        if (found == 1) {
            if ((int) pG->sceat_x17C < 0) {
                SndCall(6, 0x28, &door->pos, 0, 0, 0);
            }
            break;
        }
        if (RsfCheck(G_ROOM_ID, 10)) {
            cEmGanado* em0 = (cEmGanado*) W->ems[0];
            cEmGanado* em1 = (cEmGanado*) W->ems[1];
            cEmGanado* em2 = (cEmGanado*) W->ems[2];

            if (em0 != errEm && em0->ckFindPL() == 1) {
                found = 1;
            }
            if (em1 != errEm && em1->ckFindPL() == 1) {
                found = 1;
            }
            if (em2 != errEm && em2->ckFindPL() == 1) {
                found = 1;
            }
        }
        ((cEmDoor*) door)->setOpenLock(0);
        SceSleep(1);
    }
    i = 0;
    while (RsfCheck(G_ROOM_ID, 4) == 0) {
        cEmDoorSetCloseLock(door);
        SceSleep(1);
        if (i++ == 19) {
            SceAtSetEnable(0xB, 1);
        }
    }
    SceAtSetEnable(0xB, 0);
    ((cEmDoor*) door)->setNormal();
}

// The two windows of the first house: fenced while the ambush hunts the player.
static void r100_WindowBreakCk()
{
    cEm* win0;
    cEm* win1;

    if (getRoomEtcWindow(0, &win0, 1) == 0) {
        return;
    }
    if (getRoomEtcWindow(1, &win1, 1) == 0) {
        return;
    }
    while (RsfCheck(G_ROOM_ID, 10) == 0) {
        ((cEmWindow*) win0)->SetEnableFence(0, 0);
        ((cEmWindow*) win1)->SetEnableFence(0, 0);
        SceSleep(1);
    }
    ((cEmWindow*) win0)->SetEnableFence(1, 0);
    ((cEmWindow*) win1)->SetEnableFence(1, 0);
    while (RsfCheck(G_ROOM_ID, 4) == 0) {
        if (!(pG->sceat_x17C & 0x80000000)) {
            RsfSet(G_ROOM_ID, 4);
            if (W->ems[0] != errEm) {
                W->ems[0]->flags_3C8 |= 0x80;
            }
            if (W->ems[1] != errEm) {
                W->ems[1]->flags_3C8 |= 0x80;
            }
            if (W->ems[2] != errEm) {
                W->ems[2]->flags_3C8 |= 0x80;
            }
            break;
        }
        SceSleep(1);
    }
}

static void r100_HouseEvent_exit()
{
    pPL->setNoSuspend(0);
    EmMgr.destroy(W->emHouse);
    pG->flags_5010 &= ~0x800;
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The Ganado bursting out of the first house.
static void r100_HouseEvent()
{
    Vec pos;

    SceAtSetEnable(0x15, 0);
    if (RsfCheck(G_ROOM_ID, 15)) {
        return;
    }
    RsfSet(G_ROOM_ID, 15);
    SceEventStart(0);
    pG->flags_5010 |= 0x800;
    W->emHouse = EmSetFromList2(0x25, 1);
    W->emHouse->setNoSuspend(1);
    W->emHouse->be_flag |= 0x1000;
    W->carSub->setNoSuspend(1);
    {
        cPlayer* p = pPL;

        pos.x = -80540.0f;
        pos.y = -8.0f;
        pos.z = -12073.0f;
        p->setPos(&pos);
    }
    pos.x = 0.0f;
    pos.y = 2.99f;
    pos.z = 0.0f;
    pPL->setAng(&pos);
    BitOff(pG->flags_170, 0x10000000);
    BitOff(pG->flags_58, 0x40000000);
    pPL->setNoSuspend(1);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x34), 10, 0, 1, 0);
    SndStrReq(1, 0x22, 0x80000003, 0, 0, 0.0f);
    CamCtrl.CutCall(0xA);
    SceSetEventCancel(1, (TaskFunc) r100_HouseEvent_exit, 0, -1, 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        pG->flags_5010 |= 0x02000000;
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r100_HouseEvent_exit();
}

static Vec r100_sndPos = {-78589.0f, 860.0f, -36700.0f};
// Unreferenced zero word after the position in the original's .data (0x34).
static int r100_sndTimer = 0;

// The room stream: a random village call until the s03 event, then the chase stream while an
// ambush Ganado is near the player.
static void r100_StreanChk()
{
    int playing = 0;
    int armed = 1;
    int hold = 0;
    int cnt;
    int found;
    f32 lim = 900000000.0f;

    {
        u8 r = Rnd() % 45;
        cnt = r + 60;
    }
    while (RsfCheck(G_ROOM_ID, 3) == 0) {
        if ((int) pG->sceat_x17C < 0) {
            cnt--;
            if (cnt <= 0) {
                u8 r = Rnd() % 30;
                cnt = r * 5 + 180;
                SndCall(6, 0, &r100_sndPos, 0, 0, 0);
            }
        }
        SceSleep(1);
    }
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        armed = 0;
        hold = 1;
        while (RsfCheck(G_ROOM_ID, 10) == 0) {
            SceSleep(1);
        }
    }
    for (;;) {
        if (RsfCheck(G_ROOM_ID, 10)) {
            found = 0;
            cEmGanado* em0 = (cEmGanado*) W->ems[0];
            cEmGanado* em1 = (cEmGanado*) W->ems[1];
            cEmGanado* em2 = (cEmGanado*) W->ems[2];
            cEmGanado* em3 = (cEmGanado*) W->ems[3];
            cEmGanado* em4 = (cEmGanado*) W->ems[4];
            cEmGanado* em5 = (cEmGanado*) W->ems[5];
            cEmGanado* em6 = (cEmGanado*) W->ems[6];
            if (em0 != errEm && em0->ckFindPL() == 1 && em0->plDist2 < lim) {
                found = 1;
            }
            if (em1 != errEm && em1->ckFindPL() == 1 && em1->plDist2 < lim) {
                found = 1;
            }
            if (em2 != errEm && em2->ckFindPL() == 1 && em2->plDist2 < lim) {
                found = 1;
            }
            if (em3 != errEm && em3->ckFindPL() == 1 && em3->plDist2 < lim) {
                found = 1;
            }
            if (em4 != errEm && em4->ckFindPL() == 1 && em4->plDist2 < lim) {
                found = 1;
            }
            if (em5 != errEm && em5->ckFindPL() == 1 && em5->plDist2 < lim) {
                found = 1;
            }
            if (em6 != errEm && em6->ckFindPL() == 1 && em6->plDist2 < lim) {
                found = 1;
            }
            if (RsfCheck(G_ROOM_ID, 4)) {
                armed = 1;
            }
            if (armed == 0 && found == 1) {
                armed = 1;
            }
            if (armed == 1 && found == 0) {
                hold = 0;
            }
            if (found == 1 || hold == 1) {
                SndRoomStrStart(1, 0, 1);
                playing = 1;
            } else if (playing == 1) {
                playing = 0;
                SndRoomStrStop(3);
                SceSleep(150);
            }
        }
        SceSleep(1);
    }
}

// The player looks at the car: the s03 event, then the Ganado and the player are placed.
static void r100_Sce_look()
{
    Vec pos;
    Vec ang;
    void* evt;
    cEm* em;

    SceEventStart(0);
    SceAtSetEnable(0xA, 0);
    RsfSet(G_ROOM_ID, 3);
    pG->flags_54 |= 0x400;
    SceSleep(2);
    if (readEvent(0, 1, &evt)) {
        EvtMgr.SetEvt(evt, (u32*) 0);
        while (EvtMgr.IsAliveEvt(&EvtMgr.x34, 0, 0)) {
            SceSleep(1);
        }
        freeEvent(0, 1);
    }
    pG->flags_54 |= 0x400;
    SceSleep(2);
    em = W->em;
    if (em != 0 && em != errEm) {
        em->flags_3C8 |= 1;
        pos.x = -79116.0f;
        pos.y = 860.0f;
        pos.z = -38890.0f;
        em->setPos(&pos);
        ang.x = 0.0f;
        ang.y = -1.39f;
        ang.z = 0.0f;
        em->setAng(&ang);
        pos.x = -82910.0f;
        pos.y = 860.0f;
        pos.z = -38480.0f;
        pPL->setPos(&pos);
        ang.x = 0.0f;
        ang.y = 1.75f;
        ang.z = 0.0f;
        pPL->setAng(&ang);
    }
    pPL->cCoord::matUpdate();
    SceSleep(1);
    pG->flags_54 &= ~0x400;
    SceEventEnd(0);
}

// The s03 Ganado died: the s20 event (the truck), then the room switches to the after state.
static void r100_Sce_zombi_dead(cEm* em)
{
    Vec at[4];
    void* evt;
    Event* ev;
    EmListData* l;
    int zero;

    if (em == 0 || em == errEm) {
        pLog->err(0, 0, "r100_Sce_zombi_dead : non em");
    } else {
        em->setNoSuspend(0);
    }
    while (em->checkStatus(5) != 0) {
        SceSleep(1);
    }
    while (SceCheckEventStart() == 0) {
        SceSleep(1);
    }
    SceEventStart(0);
    SndRoomStrStop(0);
    DC.setAramSort(0);
    r100_em_set();
    if (readEvent(3, 1, &evt)) {
        EvtMgr.SetEvt(evt, (u32*) &ev);
        ev->status |= 0x800;
        while (EvtMgr.IsAliveEvt(&EvtMgr.x34, 0, 0)) {
            SceSleep(1);
        }
        freeEvent(3, 1);
    }
    zero = 0;
    pG->flags_54 |= 0x400;
    W->ems[0]->setNoSuspend(0);
    W->ems[1]->setNoSuspend(0);
    W->ems[2]->setNoSuspend(0);
    BitOn(W->ems[1]->flags_3C8, 1);
    BitOn(W->ems[2]->flags_3C8, 1);
    l = EM_LIST(4);
    l->x3 = zero;
    l = EM_LIST(5);
    l->x3 = zero;
    r100_Car_pos_move();
    SceSleep(1);
    r100_trap_set();
    at[0].x = 750.0f;
    at[0].y = 0.0f;
    at[0].z = -750.0f;
    at[1].x = 750.0f;
    at[1].y = 0.0f;
    at[1].z = 750.0f;
    at[2].x = -750.0f;
    at[2].y = 0.0f;
    at[2].z = 750.0f;
    at[3].x = -750.0f;
    at[3].y = 0.0f;
    at[3].z = -750.0f;
    if (SceAtCreateExecAt(em, at, 1, 8, 1, 1000.0f, 1, 0.0f, 0.0f, 1, 0x12, (TaskFunc) r100_MesGanado, zero, 2) == -1) {
        pLog->err(0, 0, "move : SceAt no create");
    }
    SceAtSetEnable(1, 1);
    SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) r100_GakeEvent, 0, 1);
    readEvent(4, 0, 0);
    SetSstDispFlag(0x11, 1);
    SceAtSetEnable(0xC, 0);
    SceAtSetEnable(0x19, 0);
    SceAtSetEnable(0x1A, 0);
    SceAtDataSet_exec(0x18, 0x12, 0, (TaskFunc) r100_MesBrige, 0, 1);
    SceAtSetEnable(0x1C, 1);
    SceAtSetEnable(0x1E, 0);
    RsfSet(G_ROOM_ID, 10);
    BitOn(pG->flags_51BC, 0x4000);
    BitOff(pG->flags_54, 0x400);
    DC.setAramSort(1);
    SceEventEnd(0);
    OpeSetOpenTerm(1, -81500.0f, 860.0f, -38900.0f, 1.6f);
}

// The car down in the ravine after the s20 event.
extern "C" void r100_Car_pos_move()
{
    Vec pos;
    Vec rot;

    pos.x = -120225.0f;
    pos.y = -14319.0f;
    pos.z = -2131.0f;
    rot.x = -0.159366012f;
    rot.y = 0.893207133f;
    rot.z = 3.46660805f;
    W->car->setPos(&pos);
    W->car->setAng(&rot);
    W->car->pPartsHead[11].scale.x = 0.0f;
    W->car->pPartsHead[11].scale.y = 0.0f;
    W->car->pPartsHead[11].scale.z = 0.0f;
    W->car->pPartsHead[12].scale.x = 0.0f;
    W->car->pPartsHead[12].scale.y = 0.0f;
    W->car->pPartsHead[12].scale.z = 0.0f;
    W->car->pPartsHead[13].scale.x = 0.0f;
    W->car->pPartsHead[13].scale.y = 0.0f;
    W->car->pPartsHead[13].scale.z = 0.0f;
    W->car->pPartsHead[14].scale.x = 0.0f;
    W->car->pPartsHead[14].scale.y = 0.0f;
    W->car->pPartsHead[14].scale.z = 0.0f;
    W->car->pPartsHead[16].scale.x = 0.0f;
    W->car->pPartsHead[16].scale.y = 0.0f;
    W->car->pPartsHead[16].scale.z = 0.0f;
    W->car->pPartsHead[15].scale.x = 1.0f;
    W->car->pPartsHead[15].scale.y = 1.0f;
    W->car->pPartsHead[15].scale.z = 1.0f;
    W->car->partsMatCalc();
    pos.x = -120022.0f;
    pos.y = -14319.2725f;
    pos.z = 5740.0f;
    rot.x = 5.04974413f;
    rot.y = 0.090861842f;
    rot.z = -1.1017915f;
    W->carSub->setPos(&pos);
    W->carSub->setAng(&rot);
    SceAtSetEnable(4, 0);
    SceAtSetEnable(0x26, 0);
    SceAtSetEnable(0x27, 0);
    SceAtSetEnable(5, 0);
    SceAtSetEnable(0x25, 0);
    SmdGetObjPtr(0x23)->be_flag &= ~2;
    SmdGetObjPtr(0x44)->be_flag &= ~2;
    SmdGetObjPtr(0x37)->be_flag |= 2;
    SmdGetObjPtr(0x38)->be_flag |= 2;
    if (W->cop[0]) {
        W->cop[0]->be_flag &= ~2;
    }
    if (W->cop[1]) {
        W->cop[1]->be_flag &= ~2;
    }
}

// The trap Ganados of the after state (the three at the fire get their event motions).
extern "C" void r100_trap_set()
{
    cEm* em;

    EmSetFromList2(9, 1);
    EmSetFromList2(0xA, 1);
    EmSetFromList2(0xB, 1);
    EmSetFromList2(0xC, 1);
    EmSetFromList2(0xD, 1);
    EmSetFromList2(0xE, 1);
    EmSetFromList2(0x1D, 1);
    EmSetFromList2(0x7A, 1);
    if (pG->x4F8E == 0) {
        em = EmSetFromList2(6, 1);
        if (em != errEm) {
            ((cEmGanado*) em)->setEvtMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x2E), 0, 0, 0);
        }
        em = EmSetFromList2(7, 1);
        if (em != errEm) {
            ((cEmGanado*) em)->setEvtMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x2F), 0, 0, 0);
        }
        em = EmSetFromList2(8, 1);
        if (em != errEm) {
            ((cEmGanado*) em)->setEvtMotion(ROOM_ARC_PTR(pG->pRoomArc, 0x30), 0, 0, 0);
        }
    }
}

static void r100_MesDoor()
{
    SndCall(6, 0x29, 0, 0, 0, 0);
    SceMesSet(0xB, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
}

static void r100_MesTruck()
{
    SceMesSet(0xC, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
}

static void r100_MesGanado()
{
    CamCtrl.StartLookDownEm(W->em);
    SceEventStart(1);
    W->em->setNoSuspend(1);
    SceMesSet(0xD, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
    SceEventEnd(0);
    CamCtrl.EndLookDownEm();
}

// The officer at the wheel talks (s30).
static void r100_MesCar00()
{
    void* evt;

    if (RsfCheck(G_ROOM_ID, 10)) {
        return;
    }
    W->car->setNoSuspend(1);
    W->cop[0]->setNoSuspend(1);
    W->cop[1]->setNoSuspend(0);
    pG->flags_5010 |= 0x800;
    if (W->se) {
        SndStop(W->se, 0);
    }
    if (readEvent(5, 1, &evt)) {
        EvtMgr.SetEvt(evt, (u32*) 0);
        while (EvtMgr.IsAliveEvt(&EvtMgr.x34, 0, 0)) {
            SceSleep(1);
        }
        freeEvent(5, 1);
    }
    W->cop[1]->setNoSuspend(0);
    W->cop[0]->setNoSuspend(0);
    W->car->setNoSuspend(0);
    W->carSub->setNoSuspend(0);
    pG->flags_5010 &= ~0x800;
}

// The other officer talks (s42).
static void r100_MesCar01()
{
    void* evt;

    if (RsfCheck(G_ROOM_ID, 10)) {
        return;
    }
    W->car->setNoSuspend(1);
    W->cop[1]->setNoSuspend(1);
    W->cop[0]->setNoSuspend(0);
    pG->flags_5010 |= 0x800;
    if (W->se) {
        SndStop(W->se, 0);
    }
    if (readEvent(7, 1, &evt)) {
        EvtMgr.SetEvt(evt, (u32*) 0);
        while (EvtMgr.IsAliveEvt(&EvtMgr.x34, 0, 0)) {
            SceSleep(1);
        }
        freeEvent(7, 1);
    }
    W->cop[1]->setNoSuspend(0);
    W->cop[0]->setNoSuspend(0);
    W->car->setNoSuspend(0);
    W->carSub->setNoSuspend(0);
    pG->flags_5010 &= ~0x800;
}

static void r100_MesBrige()
{
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        SceMesSet(0xF, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
    } else if (RsfCheck(G_ROOM_ID, 14) == 0) {
        r100_GakeEvent(0);
    } else {
        SceEventStart(0);
        CamCtrl.CutCall(8);
        SceMesSet(0xE, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.Comeback(0);
        SceEventEnd(0);
    }
}

// The officers on the bridge (s43).
static void r100_EventBrige()
{
    Vec pos;
    void* evt;

    if (RsfCheck(G_ROOM_ID, 10)) {
        return;
    }
    pos.x = -112251.0f;
    pos.y = -193.0f;
    pos.z = -4420.0f;
    pPL->setPos(&pos);
    W->car->setNoSuspend(1);
    W->car->x12F = 1;
    W->cop[0]->setNoSuspend(0);
    W->cop[1]->setNoSuspend(0);
    pG->flags_5010 |= 0x800;
    if (readEvent(8, 1, &evt)) {
        EvtMgr.SetEvt(evt, (u32*) 0);
        while (EvtMgr.IsAliveEvt(&EvtMgr.x34, 0, 0)) {
            SceSleep(1);
        }
    }
    freeEvent(8, 1);
    W->cop[1]->setNoSuspend(0);
    W->cop[0]->setNoSuspend(0);
    W->car->setNoSuspend(0);
    W->carSub->setNoSuspend(0);
    pG->flags_5010 &= ~0x800;
}

// TexRender blend setup of one water object.
#define R100_TEX_OBJ(id, col, v138, v136, v137) \
    obj = SmdGetObjPtr(id);                     \
    obj->pInfo->setTexBlendTbl(tbl);            \
    obj->pInfo->setBlendRatio(0xFF);            \
    obj->pInfo->color[3] = col;                 \
    obj->x136 = v136;                           \
    obj->x137 = v137;                           \
    obj->x138 = v138;

// The pond surface: a render target blended into the water objects.
extern "C" void setTexRender()
{
    cObj* obj;
    u8* tbl = r100_texTbl;

    if (GetTexRenderMgr(&W->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = W->tex->texId;
        W->tex->repType = 1;
        EstSet(0, -1, 0, 0, 1, 0, W->tex->mask | 1, 0, 0, 0);
        W->tex->sy = W->tex->sx = 0x40;
    } else {
        pLog->err(0, 0, "R100Init() : Manager alloc failed!!");
    }
    R100_TEX_OBJ(0, 0xF0, 0x80, 2, 0x1E);
    R100_TEX_OBJ(1, 0xF0, 0x80, 2, 0x1E);
}

extern "C" void Evt_R100S40_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        break;
    case 1:
        pG->flags_5010 |= 0x02000000;
        if (e->cut == 0 && e->frame == 0) {
            EventCarInit(e);
        }
        break;
    case 2:
        break;
    case 3:
        pG->flags_51C0 |= 0x10;
        break;
    }
}

extern "C" void Evt_R100S20_Func(Event* e)
{
    void* mod;

    if (e->funcMode == 1) {
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "obm2d00", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x10;
                }
            }
            break;
        case 2:
            if (e->frame == 0) {
                if (!(pG->flags_60 & 0x02000000)) {
                    W->ems[1]->setNoSuspend(1);
                    W->ems[2]->setNoSuspend(1);
                }
            }
            break;
        }
    }
}

extern "C" void Evt_R100S03_Func(Event* e)
{
    void* mod;

    if (e->funcMode == 1) {
        switch (e->cut) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "wep0200", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
            }
            break;
        default:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "wep0200", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
            break;
        }
    }
}

// The skull message: the room stream ducks under the voice stream.
static void r100_mes_gaikotu_bgm()
{
    u32 id;

    SndRoomStrVolSet(1, 200);
    id = SndStrReq(0, 0x12, 3, 0, 0, 0.0f);
    SceSleep(1);
    while (SndStrStatusCk(id, 0x16) != 0 && !(pG->flags_174 & 0x20000000)) {
        SceSleep(1);
    }
    SndStrReq(id, 8, 0, 0);
    SndRoomStrVolSet(0x2D, 200);
}

static void r100_mes_gaikotu()
{
    pG->flags_174 &= ~0x20000000;
    SceExec(0x12, (TaskFunc) r100_mes_gaikotu_bgm, 0, 2, 2, 0);
    SceUpCut(0x2F, 0xB, -1, 0);
    pG->flags_174 |= 0x20000000;
}

static void r100_mes_gaikotu_bgm_down()
{
    SndRoomStrVolSet(0x2D, 400);
}

static void r100_mes_gaikotu_bgm_up()
{
    SndRoomStrVolReset(400);
}
