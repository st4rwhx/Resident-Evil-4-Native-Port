#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "flag_rsf.h"
#include "event.h"
#include "global.h"
#include "game.h"
#include "datactrl.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "emtree.h"
#include "emtorch.h"
#include "etc_model.h"
#include "esp.h"
#include "est.h"
#include "player.h"
#include "cockpit.h"
#include "snd.h"
#include "rnd.h"
#include "debug.h"

// Room 1-19 (D:/Bio4/Prog/r119.cpp): the village square with the giant; the three huts and their
// roofs the giant breaks, the trees, the thunder lights and the giant / dog / parasite events.

struct R119Work {
    cEm* golem;         // 0x00  the giant
    cEm* dog;           // 0x04  the dog set by the dog event
    u8 pad_8[0x48 - 0x8];
    cSat* sat[3];       // 0x48  hut A / B / C collision
    cSat* eat[3];       // 0x54  ... attribute collision
};

static R119Work* r119_work;

// Pointer store through a reference: the work pointer is reloaded after it.
static inline void PSet(cEm*& d, cEm* v) { d = v; }
// The six collision stores are reference stores too: the following `pG` load stays below the
// `stw` into the work (a plain member store lets ours hoist it, which shifts the `addi` pairs of
// the pos/rot table addresses apart and ties the rot/pos `lis` pseudos' live lengths).
static inline void PSetSat(cSat*& d, cSat* v) { d = v; }

static Vec r119_koyaPos[3] = {
    {112971.0f, 2262.0f, 16941.0f}, {117073.0f, 2262.0f, 17411.0f}, {121549.0f, 2262.0f, 15823.0f},
};
static Vec r119_koyaRot[3] = {
    {0.0f, -3.1642818f, 0.0f}, {0.0f, -3.1642818f, 0.0f}, {0.0f, -4.00204f, 0.0f},
};
// thunder light powers (the tool state callbacks)
static f32 r119_lightPow2A = 1.0f;
static f32 r119_lightPow6A = 0.95f;
static f32 r119_lightPow2B = 1.5f;
static f32 r119_lightPow6B = 1.5f;
static f32 r119_lightDist = 10000.0f;
static f32 r119_lightRange = 2000.0f;
static f32 r119_lightAng = 1.5707964f;
static f32 r119_lightAng2 = 1.0471976f;

// The giant (enemy 0x28) by vtable slot.
class cEmGolem : public cEm {
public:
    virtual void setDogPos(Vec* pos, f32 ang);   // 0x50
    virtual void setDie();        // 0x58
    virtual int ckEvent();        // 0x60
    virtual void v68();
    virtual int ckBusy();         // 0x70
};

static void r119_ThunderFlagOn();
static void r119_ThunderFlagOff();
static void r119_ThunderMove();
static void r119_EventGolemAppear();
static void r119_EventParasiet();
static void r119_EventDogAppear();
static void koya_destroy_check();
extern "C" void koyaA_destroy();
extern "C" void koyaB_destroy();
extern "C" void koyaC_destroy();
extern "C" void YaneA_destroy();
extern "C" void YaneB_destroy();
extern "C" void YaneC_destroy();
extern "C" void koyaA_delete();
extern "C" void koyaB_smd_delete();
extern "C" void koyaB_delete();
extern "C" void koyaC_delete();
extern "C" void YaneA_delete();
extern "C" void YaneB_smd_delete();
extern "C" void YaneB_delete();
extern "C" void YaneC_delete();
extern "C" void koya_init();
extern "C" void Evt_R119S00_Func(Event* e);
extern "C" void Evt_R119S10_Func(Event* e);
extern "C" void Evt_R119S20_Func(Event* e);

// Third SetTree block: the pRoomArc read goes through the struct view `pGS` so the `lwz pG` depends
// on the preceding pos/rot stores (a plain `pG` load is a fixed scalar that sched2 hoists above them).
void R119Init()
{
    Vec pos;
    Vec rot;
    cEmTree* tree;
    cObj* obj;

    DC.setAramSort(0);
#line 95 "D:/Bio4/Prog/r119.cpp"
    r119_work = (R119Work*) MEM_CALLOC(sizeof(R119Work), 1, 0xd);

    EvtMgr.EvtReadAram("event/evd/r119s00.evd", 0, 0, 0, 0);
    EvtMgr.SetFunc("evt_r119s00_func", (void*) Evt_R119S00_Func);
    EvtMgr.SetFunc("evt_r119s10_func", (void*) Evt_R119S10_Func);
    EvtMgr.SetFunc("evt_r119s20_func", (void*) Evt_R119S20_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) r119_EventGolemAppear, 0, 1);
        SmdSetTrans(0x2C, 0);
        SceAtSetEnable(5, 0);
    } else {
        SceAtSetEnable(5, 1);
        SmdSetTrans(0x24, 0);
        SmdSetTrans(0x25, 0);
    }
    SmdGetObjPtr(0x2C)->be_flag |= 0x20;
    SmdGetObjPtr(0x25)->be_flag |= 0x20;
    SmdGetObjPtr(0x24)->be_flag |= 0x20;
    SceExec(0x12, (TaskFunc) koya_destroy_check, 0, 0, 2, 0);
    PSetSat(r119_work->sat[0], SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x1F), 0, &r119_koyaPos[0], &r119_koyaRot[0], 0));
    PSetSat(r119_work->sat[1], SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x1F), 0, &r119_koyaPos[1], &r119_koyaRot[1], 0));
    PSetSat(r119_work->sat[2], SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x1F), 0, &r119_koyaPos[2], &r119_koyaRot[2], 0));
    PSetSat(r119_work->eat[0], EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x20), 0, &r119_koyaPos[0], &r119_koyaRot[0], 0));
    PSetSat(r119_work->eat[1], EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x20), 0, &r119_koyaPos[1], &r119_koyaRot[1], 0));
    PSetSat(r119_work->eat[2], EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x20), 0, &r119_koyaPos[2], &r119_koyaRot[2], 0));
    koya_init();
    if (RsfCheck(G_ROOM_ID, 4)) {
        YaneA_delete();
    }
    if (RsfCheck(G_ROOM_ID, 5)) {
        YaneB_delete();
    }
    if (RsfCheck(G_ROOM_ID, 6)) {
        YaneC_delete();
    }
    if (RsfCheck(G_ROOM_ID, 1)) {
        koyaA_delete();
    }
    if (RsfCheck(G_ROOM_ID, 2)) {
        koyaB_delete();
    }
    if (RsfCheck(G_ROOM_ID, 3)) {
        koyaC_delete();
    }
    SceExec(0x12, (TaskFunc) r119_ThunderMove, 0, 0, 2, 0);
    SceAtSetEnable(3, 0);
    SceAtSetEnable(4, 0);
    pos.x = 108540.0f;
    pos.y = 2350.0f;
    pos.z = 9387.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    tree = SetTree(ROOM_ARC_PTR(pG->pRoomArc, 0x22), ROOM_ARC_PTR(pG->pRoomArc, 0x23), &pos, &rot);
    tree->lightInfo.x54 &= ~0x10000;
    pos.x = 109167.0f;
    pos.y = 2350.0f;
    pos.z = 18073.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    tree = SetTree(ROOM_ARC_PTR(pG->pRoomArc, 0x22), ROOM_ARC_PTR(pG->pRoomArc, 0x23), &pos, &rot);
    tree->lightInfo.x54 &= ~0x10000;
    pos.x = 123623.0f;
    pos.y = 2350.0f;
    pos.z = 8190.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    tree = SetTree(ROOM_ARC_PTR(pGS->pRoomArc, 0x22), ROOM_ARC_PTR(pGS->pRoomArc, 0x23), &pos, &rot);
    tree->lightInfo.x54 &= ~0x10000;
    if ((obj = SmdGetObjPtr(0x21)) != 0) {
        Vec ang = {-0.21598449f, -1.4628042f, -2.1205752f};

        obj->setAng(&ang);
    }
    if ((obj = SmdGetObjPtr(0x22)) != 0) {
        Vec ang = {-1.259219f, 1.5707964f, 1.259219f};

        obj->setAng(&ang);
    }
}

void R119Main()
{
    SmdGetObjPtr(0x24);
}

// Lightning on: the two hut lights brighten (per tool state).
static void r119_ThunderFlagOn()
{
    cLight* l;

    if (EffGetToolState() == 1) {
        l = LightMgr.getWorkPtr(2);
        l->power = r119_lightPow2B;
        l = LightMgr.getWorkPtr(6);
        l->power = r119_lightPow6B;
    } else if (EffGetToolState() == 2) {
        l = LightMgr.getWorkPtr(2);
        l->power = r119_lightPow2A;
        l = LightMgr.getWorkPtr(6);
        l->power = r119_lightPow6A;
    }
}

static void r119_ThunderFlagOff()
{
    cLight* l;

    l = LightMgr.getWorkPtr(2);
    l->power = 0.509f;
    l = LightMgr.getWorkPtr(6);
    l->power = 0.897f;
}

// Thunder every 240..385 frames.
static void r119_ThunderMove()
{
    int cnt;

    SceSleep(1);
    {
        u8 r = Rnd() % 30;
        cnt = r * 5 + 240;
    }
    EffSetToolStateCallBack(0, r119_ThunderFlagOn, r119_ThunderFlagOff);
    for (;;) {
        if (cnt == 0) {
            EstSet(0, -1, 0, 0, 1, 0, 1, 0, 0, 0);
            {
                u8 r = Rnd() % 30;
                cnt = r * 5 + 240;
            }
            SceSndCallThunder();
        }
        cnt--;
        SceSleep(1);
    }
}

// The giant appears; the fight runs until it dies, with the dog and the parasite events.
static void r119_EventGolemAppear()
{
    cObj* obj;

    RsfSet(G_ROOM_ID, 0);
    pG->flags_5010 |= 0x800;
    EvtMgr.EvtReadExec("event/evd/r119s00.evd", 0, 0);
    EvtMgr.EvtReadAram("event/evd/r119s10.evd", 0, 0, 0, 0);
    EvtMgr.EvtReadAram("event/evd/r119s20.evd", 0, 0, 0, 0);
    EvtMgr.EvtReadAram("event/evd/r119s30.evd", 0, 0, 0, 0);
    r119_work->golem = EmSetFromList2(0x28, 0);
    GamePointBossReset();
    Cckpt.life.flags = (u32) r119_work->golem;
    BitOff(pG->flags_5010, 0x800);
    {
        Vec v;

        v.x = 0.0f;
        v.y = 3.03f;
        v.z = 0.0f;
        pPL->setAng(&v);
    }
    BitOff(pG->door_flags_51CC, 0x80);
    BitOff(pG->door_flags_51CC, 4);
    BitOff(pG->door_flags_51D0, 0x10000000);
    BitOff(pG->door_flags_51D0, 0x10000000);
    if ((obj = SmdGetObjPtr(0x21)) != 0) {
        Vec ang = {0.0f, -1.51458f, 0.0f};

        obj->setAng(&ang);
    }
    if ((obj = SmdGetObjPtr(0x22)) != 0) {
        Vec ang = {0.0f, 1.60316f, 0.0f};

        obj->setAng(&ang);
    }
    SceAtSetEnable(3, 1);
    SceAtSetEnable(4, 1);
    SceAtSetEnable(5, 1);
    u32 cnt = 0;
    for (;;) {
        int stat;
        cPlayer* pl;

        stat = r119_work->golem->checkStatus(5);
        if (stat == 0) {
            RsfSet(G_ROOM_ID, 7);
            SndRoomStrStop(3);
            pG->flags_5010 |= 0x800;
            EvtMgr.EvtReadExec("event/evd/r119s20.evd", 0x2B, 0);
            SceAtSetEnable(3, 0);
            SceAtSetEnable(4, 0);
            BitOff(pG->flags_5010, 0x800);
            ((cEmGolem*) r119_work->golem)->setDie();
            EstSet((int) r119_work->golem, -1, 0, 0, 1, 0xF, 0, 0, (u32) r119_work->golem, (void*) stat);
            if (r119_work->dog != 0) {
                EmMgr.destroy(r119_work->dog);
            }
            BitOn(pG->door_flags_51CC, 0x80);
            BitOn(pG->door_flags_51CC, 4);
            BitOn(pG->door_flags_51D0, 0x10000000);
            BitOn(pG->door_flags_51D0, 0x10000000);
            if ((obj = SmdGetObjPtr(0x21)) != 0) {
                Vec ang = {-0.21598449f, -1.4628042f, -2.1205752f};

                obj->setAng(&ang);
            }
            if ((obj = SmdGetObjPtr(0x22)) != 0) {
                Vec ang = {-1.259219f, 1.5707964f, 1.259219f};

                obj->setAng(&ang);
            }
            return;
        }
        cnt++;
        pl = pPL;
        if ((pG->flags_51BC & 0x00080000) && !(pG->flags_174 & 0x02000000)) {
            SceDebugDisp("CNT[%d/%d]", cnt, 900);
            if (pl->checkEvent() == 1) {
                SceDebugDisp("PL[OK]");
            } else {
                SceDebugDisp("PL[NO]");
            }
            if (!(r119_work->golem->flags_3C8 & 0x10) && ((cEmGolem*) r119_work->golem)->ckBusy() == 0) {
                SceDebugDisp("EM[OK]");
            } else {
                SceDebugDisp("EM[NO]");
            }
            if (cnt > 899) {
                cObj* o;

                for (o = ObjMgr.pAlive; o != 0; o = (cObj*) o->next) {
                    if (o->id == 0x1A || o->id == 0x29 || o->id == 0x2A || (*(u32*) &o->id & 0xFFFF0000) == 0x22010000) {
                        cnt = 600;
                    }
                }
            }
            if (cnt > 900 && pl->checkEvent() == 1 && !(r119_work->golem->flags_3C8 & 0x10) && ((cEmGolem*) r119_work->golem)->ckBusy() == 0) {
                pG->flags_174 |= 0x02000000;
                SceExec(0x12, (TaskFunc) r119_EventDogAppear, 0, 0, 2, 0);
            }
        }
        if ((((cEmGolem*) r119_work->golem)->ckEvent() != 0 && !(pG->flags_174 & 0x01000000)) || DebugTrg(0) != 0) {
            pG->flags_174 |= 0x01000000;
            SceExec(0x12, (TaskFunc) r119_EventParasiet, 0, 0, 2, 0);
        }
        SceSleep(1);
    }
}

static void r119_EventParasiet()
{
    pG->flags_5010 |= 0x800;
    EvtMgr.EvtReadExec("event/evd/r119s30.evd", 0x2B, 0xA0);
    pG->flags_5010 &= ~0x800;
}

// The dog comes to help: its event, then Leon and the dog are placed.
static void r119_EventDogAppear()
{
    Vec pos;
    Vec ang;

    pG->flags_5010 |= 0x800;
    EvtMgr.EvtReadExec("event/evd/r119s10.evd", 0x2B, 0);
    pG->flags_5010 &= ~0x800;
    PSet(r119_work->dog, EmSetFromList2(0x29, 0));
    pos.x = 116292.0f;
    pos.y = 2298.0f;
    pos.z = 4229.0f;
    ((cEmGolem*) r119_work->golem)->setDogPos(&pos, -0.47f);
    pos.x = 113872.0f;
    pos.y = 2298.0f;
    pos.z = 8485.0f;
    pPL->setPos(&pos);
    ang.x = 0.0f;
    ang.y = 2.77f;
    ang.z = 0.0f;
    pPL->setAng(&ang);
}

// The huts and roofs fall when their event flags come up.
static void koya_destroy_check()
{
    static const Vec r119_up = {0.0f, 1.0f, 3.1415927f};

    for (;;) {
        if ((pG->flags_174 & 0x80000000) && RsfCheck(G_ROOM_ID, 1) == 0) {
            RsfSet(G_ROOM_ID, 1);
            koyaA_destroy();
        } else if ((pG->flags_174 & 0x10000000) && RsfCheck(G_ROOM_ID, 4) == 0 && RsfCheck(G_ROOM_ID, 1) == 0) {
            RsfSet(G_ROOM_ID, 4);
            YaneA_destroy();
        }
        if ((pG->flags_174 & 0x40000000) && RsfCheck(G_ROOM_ID, 2) == 0) {
            RsfSet(G_ROOM_ID, 2);
            koyaB_destroy();
        } else if ((pG->flags_174 & 0x08000000) && RsfCheck(G_ROOM_ID, 5) == 0 && RsfCheck(G_ROOM_ID, 2) == 0) {
            RsfSet(G_ROOM_ID, 5);
            YaneB_destroy();
        }
        if ((pG->flags_174 & 0x20000000) && RsfCheck(G_ROOM_ID, 3) == 0) {
            RsfSet(G_ROOM_ID, 3);
            koyaC_destroy();
        } else if ((pG->flags_174 & 0x04000000) && RsfCheck(G_ROOM_ID, 6) == 0 && RsfCheck(G_ROOM_ID, 3) == 0) {
            RsfSet(G_ROOM_ID, 6);
            YaneC_destroy();
        }
        SceSleep(1);
    }
}

extern "C" void koyaA_destroy()
{
    Vec pos = {113011.0f, 2270.0f, 16997.0f};
    Vec rot = {0.0f, 3.1415927f, 0.0f};
    cEm* torch;

    SndCall(8, 0x1B, &pos, 0x2B, 0, 0);
    if (RsfCheck(G_ROOM_ID, 4)) {
        EstSet(0, -1, &pos, &rot, 1, 5, 0, 0, 0, 0);
    } else {
        EstSet(0, -1, &pos, &rot, 1, 3, 0, 0, 0, 0);
    }
    koyaA_delete();
    if (getRoomEtcTorch(0, &torch, 1)) {
        ((cEmTorch*) torch)->setBreak();
    }
}

extern "C" void koyaB_destroy()
{
    Vec pos = {117111.0f, 2270.0f, 17477.0f};
    Vec rot = {0.0f, 3.1415927f, 0.0f};
    cEm* torch;

    SndCall(8, 0x1B, &pos, 0x2B, 0, 0);
    if (RsfCheck(G_ROOM_ID, 5)) {
        EstSet(0, -1, &pos, &rot, 1, 5, 0, 0, 0, 0);
    } else {
        EstSet(0, -1, &pos, &rot, 1, 3, 0, 0, 0, 0);
    }
    koyaB_delete();
    if (getRoomEtcTorch(1, &torch, 1)) {
        ((cEmTorch*) torch)->setBreak();
    }
}

extern "C" void koyaC_destroy()
{
    Vec pos = {121560.0f, 2270.0f, 15877.0f};
    Vec rot = {0.0f, 2.268928f, 0.0f};
    cEm* torch;

    SndCall(8, 0x1B, &pos, 0x2B, 0, 0);
    if (RsfCheck(G_ROOM_ID, 6)) {
        EstSet(0, -1, &pos, &rot, 1, 5, 0, 0, 0, 0);
    } else {
        EstSet(0, -1, &pos, &rot, 1, 3, 0, 0, 0, 0);
    }
    koyaC_delete();
    if (getRoomEtcTorch(2, &torch, 1)) {
        ((cEmTorch*) torch)->setBreak();
    }
}

extern "C" void YaneA_destroy()
{
    Vec pos = {113011.0f, 2270.0f, 16997.0f};
    Vec rot = {0.0f, 3.1415927f, 0.0f};

    SndCall(8, 0x1B, &pos, 0x2B, 0, 0);
    EstSet(0, -1, &pos, &rot, 1, 4, 0, 0, 0, 0);
    YaneA_delete();
}

extern "C" void YaneB_destroy()
{
    Vec pos = {117111.0f, 2270.0f, 17477.0f};
    Vec rot = {0.0f, 3.1415927f, 0.0f};

    SndCall(8, 0x1B, &pos, 0x2B, 0, 0);
    EstSet(0, -1, &pos, &rot, 1, 4, 0, 0, 0, 0);
    YaneB_delete();
}

extern "C" void YaneC_destroy()
{
    Vec pos = {121560.0f, 2270.0f, 15877.0f};
    Vec rot = {0.0f, 2.268928f, 0.0f};

    SndCall(8, 0x1B, &pos, 0x2B, 0, 0);
    EstSet(0, -1, &pos, &rot, 1, 4, 0, 0, 0, 0);
    YaneC_delete();
}

extern "C" void koyaA_delete()
{
    SatMgr.destroy(r119_work->sat[0]);
    EatMgr.destroy(r119_work->eat[0]);
    SmdSetTrans(1, 0);
    SmdSetTrans(4, 0);
    SmdSetTrans(5, 0);
    SmdSetTrans(0x27, 0);
    SmdSetTrans(0x26, 0);
    SceAtSetEnable(0x98, 0);
    SceAtSetEnable(0x99, 0);
}

extern "C" void koyaB_smd_delete()
{
    SmdSetTrans(2, 0);
    SmdSetTrans(6, 0);
    SmdSetTrans(7, 0);
    SmdSetTrans(0x29, 0);
    SmdSetTrans(0x28, 0);
}

extern "C" void koyaB_delete()
{
    SatMgr.destroy(r119_work->sat[1]);
    EatMgr.destroy(r119_work->eat[1]);
    koyaB_smd_delete();
    SceAtSetEnable(0x97, 0);
}

extern "C" void koyaC_delete()
{
    SatMgr.destroy(r119_work->sat[2]);
    EatMgr.destroy(r119_work->eat[2]);
    SmdSetTrans(3, 0);
    SmdSetTrans(8, 0);
    SmdSetTrans(9, 0);
    SmdSetTrans(0x2B, 0);
    SmdSetTrans(0x2A, 0);
    SceAtSetEnable(0x93, 0);
    SceAtSetEnable(0x94, 0);
    SceAtSetEnable(0x95, 0);
}

extern "C" void YaneA_delete()
{
    EatMgr.destroy(r119_work->eat[0]);
    r119_work->eat[0] = EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x21), 0, &r119_koyaPos[0], &r119_koyaRot[0], 0);
    SmdSetTrans(4, 0);
    SmdSetTrans(5, 0);
    SmdSetTrans(0x27, 1);
    SmdSetTrans(0x26, 1);
}

extern "C" void YaneB_smd_delete()
{
    EatMgr.destroy(r119_work->eat[1]);
    r119_work->eat[1] = EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x21), 0, &r119_koyaPos[1], &r119_koyaRot[1], 0);
    SmdSetTrans(6, 0);
    SmdSetTrans(7, 0);
    SmdSetTrans(0x29, 1);
    SmdSetTrans(0x28, 1);
}

extern "C" void YaneB_delete()
{
    YaneB_smd_delete();
}

extern "C" void YaneC_delete()
{
    EatMgr.destroy(r119_work->eat[2]);
    r119_work->eat[2] = EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x21), 0, &r119_koyaPos[2], &r119_koyaRot[2], 0);
    SmdSetTrans(8, 0);
    SmdSetTrans(9, 0);
    SmdSetTrans(0x2B, 1);
    SmdSetTrans(0x2A, 1);
}

extern "C" void koya_init()
{
    SmdSetTrans(1, 1);
    SmdSetTrans(4, 1);
    SmdSetTrans(5, 1);
    SmdSetTrans(0x27, 0);
    SmdSetTrans(0x26, 0);
    SmdSetTrans(2, 1);
    SmdSetTrans(6, 1);
    SmdSetTrans(7, 1);
    SmdSetTrans(0x29, 0);
    SmdSetTrans(0x28, 0);
    SmdSetTrans(3, 1);
    SmdSetTrans(8, 1);
    SmdSetTrans(9, 1);
    SmdSetTrans(0x2B, 0);
    SmdSetTrans(0x2A, 0);
}

static inline void r119_evtSetGiant(Event* e, char* name)
{
    void* em;

    if (e->GetMod(&em, name, 0, 0) == 1) {
        ((cEm*) em)->be_flag |= 0x10;
    }
}

static inline void r119_evtBridgeOn()
{
    SmdSetTrans(6, 1);
    SmdSetTrans(7, 1);
    SmdSetTrans(0xC, 1);
    SmdSetTrans(0xD, 1);
    SmdSetTrans(0xE, 1);
    SmdSetTrans(0xF, 1);
    SmdSetTrans(0x30, 1);
    setRoomEtcDisp(1, 1, 1);
    SmdSetTrans(0x1D, 1);
    SmdSetTrans(0, 1);
    SmdSetTrans(0x1C, 1);
    SmdSetTrans(0x1B, 1);
    SmdSetTrans(0x1F, 1);
    SmdSetTrans(0x2D, 1);
    SmdSetTrans(8, 1);
    SmdSetTrans(9, 1);
}

extern "C" void Evt_R119S00_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;
    SmdWork* w;

    switch (e->funcMode) {
    case 0:
        if (RsfCheck(G_ROOM_ID, 5)) {
            YaneB_smd_delete();
        }
        if (RsfCheck(G_ROOM_ID, 2)) {
            koyaB_smd_delete();
        }
        break;
    case 1:
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                if ((obj = SmdGetObjPtr(0x21)) != 0) {
                    e->SetMod("scr0000", obj, 5, 0, 2, 0);
                    obj->setPos(&pos);
                    obj->setAng(&rot);
                    obj->be_flag |= 0x20;
                    e->EspSetModelPtr(obj);
                }
                if ((obj = SmdGetObjPtr(0x22)) != 0) {
                    e->SetMod("scr0100", obj, 5, 0, 2, 0);
                    obj->setPos(&pos);
                    obj->setAng(&rot);
                    obj->be_flag |= 0x20;
                    e->EspSetModelPtr(obj);
                }
                if ((obj = SmdGetObjPtr(0x25)) != 0) {
                    e->SetMod("scr0200", obj, 5, 0, 2, 0);
                    obj->setPos(&pos);
                    obj->setAng(&rot);
                    obj->be_flag |= 0x20;
                    e->EspSetModelPtr(obj);
                }
                if ((obj = SmdGetObjPtr(0x24)) != 0) {
                    e->SetMod("scr0300", obj, 5, 0, 2, 0);
                    obj->setPos(&pos);
                    obj->setAng(&rot);
                    obj->be_flag |= 0x20;
                    e->EspSetModelPtr(obj);
                }
                r119_evtSetGiant(e, "em2b00");
            }
            break;
        case 0xC:
            SmdSetTrans(0x2C, 0);
            SmdSetTrans(0x24, 1);
            SmdSetTrans(0x25, 1);
            break;
        case 0xD:
            SmdSetTrans(0x2C, 1);
            SmdSetTrans(0x24, 0);
            SmdSetTrans(0x25, 0);
        case 0x10:
            if (e->frame == 0) {
                SmdSetTrans(6, 0);
                SmdSetTrans(7, 0);
                SmdSetTrans(0xC, 0);
                SmdSetTrans(0xD, 0);
                SmdSetTrans(0xE, 0);
                SmdSetTrans(0xF, 0);
                SmdSetTrans(0x30, 0);
                setRoomEtcDisp(1, 0, 1);
            }
            break;
        case 0xE:
            if (e->frame == 0) {
                SmdSetTrans(0x1D, 0);
            }
            break;
        case 0x13:
            if (e->frame == 0) {
                SmdSetTrans(0, 0);
                SmdSetTrans(0x1C, 0);
                SmdSetTrans(0x1B, 0);
                SmdSetTrans(0x2D, 0);
                SmdSetTrans(0x1F, 0);
            }
            break;
        case 0x14:
            if (e->frame == 0) {
                SmdSetTrans(0x1B, 0);
                SmdSetTrans(0x2D, 0);
            }
            break;
        case 0x17:
            if (e->frame == 0) {
                SmdSetTrans(6, 0);
                SmdSetTrans(7, 0);
                SmdSetTrans(8, 0);
                SmdSetTrans(9, 0);
            }
            break;
        default:
            if ((int) pG->flags_60 >= 0 && e->frame == 0) {
                r119_evtBridgeOn();
            }
            break;
        }
        break;
    case 2:
        SmdSetTrans(0x2C, 1);
        SmdSetTrans(0x24, 0);
        SmdSetTrans(0x25, 0);
        if ((int) pG->flags_60 >= 0) {
            r119_evtBridgeOn();
        }
        w = SmdGetWorkPtr(0x21);
        if ((obj = SmdGetObjPtr(0x21)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        w = SmdGetWorkPtr(0x22);
        if ((obj = SmdGetObjPtr(0x22)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        w = SmdGetWorkPtr(0x24);
        if ((obj = SmdGetObjPtr(0x24)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        w = SmdGetWorkPtr(0x25);
        if ((obj = SmdGetObjPtr(0x25)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        break;
    }
}

extern "C" void Evt_R119S10_Func(Event* e)
{
    if (e->funcMode == 1 && e->cut == 0 && e->frame == 0) {
        r119_evtSetGiant(e, "em2b00");
    }
}

extern "C" void Evt_R119S20_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;
    SmdWork* w;

    switch (e->funcMode) {
    case 0:
        break;
    case 1:
        if (e->cut == 0 && e->frame == 0) {
            if ((obj = SmdGetObjPtr(0x21)) != 0) {
                e->SetMod("scr0000", obj, 5, 0, 2, 0);
                obj->setPos(&pos);
                obj->setAng(&rot);
                obj->be_flag |= 0x20;
                e->EspSetModelPtr(obj);
            }
            if ((obj = SmdGetObjPtr(0x22)) != 0) {
                e->SetMod("scr0100", obj, 5, 0, 2, 0);
                obj->setPos(&pos);
                obj->setAng(&rot);
                obj->be_flag |= 0x20;
                e->EspSetModelPtr(obj);
            }
            if ((obj = SmdGetObjPtr(0x25)) != 0) {
                e->SetMod("scr0200", obj, 5, 0, 2, 0);
                obj->setPos(&pos);
                obj->setAng(&rot);
                obj->be_flag |= 0x20;
                e->EspSetModelPtr(obj);
            }
            if ((obj = SmdGetObjPtr(0x24)) != 0) {
                e->SetMod("scr0300", obj, 5, 0, 2, 0);
                obj->setPos(&pos);
                obj->setAng(&rot);
                obj->be_flag |= 0x20;
                e->EspSetModelPtr(obj);
            }
            r119_evtSetGiant(e, "em2b00");
        }
        break;
    case 2:
        w = SmdGetWorkPtr(0x21);
        if ((obj = SmdGetObjPtr(0x21)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        w = SmdGetWorkPtr(0x22);
        if ((obj = SmdGetObjPtr(0x22)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        w = SmdGetWorkPtr(0x24);
        if ((obj = SmdGetObjPtr(0x24)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        w = SmdGetWorkPtr(0x25);
        if ((obj = SmdGetObjPtr(0x25)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        {
            cObj* o;

            if ((o = SmdGetObjPtr(0x21)) != 0) {
                Vec ang = {-0.21598449f, -1.4628042f, -2.1205752f};

                o->setAng(&ang);
            }
            if ((o = SmdGetObjPtr(0x22)) != 0) {
                Vec ang = {-1.259219f, 1.5707964f, 1.259219f};

                o->setAng(&ang);
            }
        }
        break;
    }
}
