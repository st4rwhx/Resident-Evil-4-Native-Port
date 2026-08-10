#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "mes.h"
#include "fade.h"
#include "snd.h"

// Room 2-10 (D:/Bio4/Prog/r210.cpp): the lift platform of the mine (r222 shares the code), the
// mine cart ride to and from r212, and Ashley's follow / wait areas.

struct R210Work {
    u8 dummy;
};

static R210Work* r210_work;

// Collision flag bits set through a raw (non-struct) store at the info's address: the following
// `pPL` load stays below it (pl_npc.cpp AtariOnRaw).
static inline void AtariOnRaw(cAtariInfo* at, u16 b) { *(u16*) ((u8*) at + 0x1a) |= b; }

// pl_npc.cpp: MotionMove is called with a second argument by the partner code.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// `flags &= 0xEFFF` through a reference with a u16 mask: a halfword `andi.` (BitOff16's promoted
// `~b` gives a word mask) whose store keeps the following `pSUB` load below it.
static inline void U16And(u16& d, u16 mask) { d &= mask; }
// Routine bytes through int parameters: one SI zero pseudo, the stores issued ff, fc, fd, fe.
static inline void EmRoutineSet(cEm* p, int fc, int fd, int fe, int ff)
{
    p->xFC = fc;
    p->xFD = fd;
    p->xFE = fe;
    p->xFF = ff;
}

static f32 r210_daiZ = -32012.0f;
static f32 r210_daiRotGo = -0.04f;
static f32 r210_daiZGo = -32012.0f;
static f32 r210_daiRotRet = 0.04f;
static f32 r210_daiZRet = -17345.0f;
static Vec r210_torokoGoPos0 = {-18606.17f, -2200.0f, -10500.0f};
static Vec r210_torokoGoPos1 = {-60406.152f, -2200.0f, 244499.9f};
static Vec r210_torokoRetPos0 = {-38271.953f, -2200.0f, -10559.35f};
static Vec r210_torokoRetPos1 = {-79771.914f, -2200.0f, 244440.62f};

static void asl_wait();
static void asl_chase();
static void r222_DummyDoorProc();
static void r222_dai_set();
static void funcAshley2(cEm* p);
static void r222_dai_go();
static void r222_dai_ret();
static void toroko_go(int dir);
static void toroko_ret(int dir);
static void plemRide(cPlayer* pl);

// Ashley waits at the cart (area 9) / follows again (area 0xA).
static void asl_wait()
{
    SubCharCtrl(7, 0);
    SceAtSetEnable(0xA, 1);
    SceAtSetEnable(9, 0);
}

static void asl_chase()
{
    SubCharCtrl(1, 0);
    SceAtSetEnable(0xA, 0);
    SceAtSetEnable(9, 1);
}

void R210Init()
{
#line 53 "D:/Bio4/Prog/r210.cpp"
    r210_work = (R210Work*) MEM_CALLOC(sizeof(R210Work), 1, 0xd);
    if (pG->room_id_prev == 0xFFF) {
        if ((pG->flags_5018 & 0x04000000) == 0) {
            pG->flags_5018 |= 0x04000000;
        }
    }
    {
        u32 flags = pG->flags_5018;

        if (flags & 0x04000000) {
            SubCharInit(1, &pPL->pos, pPL->rot.y);
            SubCharCtrl(1, 0);
            pG->flags_51BC &= ~0x80;
        }
    }
    if ((pG->flags_54 & 0x100) == 0 && pG->room_id_prev == 0x222) {
        if ((pG->flags_51C0 & 0x40) == 0) {
            SubCharInit(1, &pPL->pos, pPL->rot.y);
            BitOn(pG->flags_5018, 0x04000000);
            if (pSUB) {
                Vec v;

                v.x = 0.0f;
                v.y = 0.0f;
                v.z = -14875.0f;
                pSUB->setPos(&v);
                {
                    cSubChar* sub = pSUB;

                    v.x = 0.0f;
                    v.y = 3.14f;
                    v.z = 0.0f;
                    sub->setAng(&v);
                }
                SubCharCtrl(7, 0);
            }
            pG->flags_51BC |= 0x80;
        }
        SmdGetObjPtr(0x20)->be_flag |= 0x20;
        SmdGetObjPtr(0x21)->be_flag |= 0x20;
        SmdGetObjPtr(0x21)->pos.z = r210_daiZ;
        SmdGetObjPtr(0x20)->pos.z = r210_daiZ;
    }
    SceAtDataSet_exec(0, 0x12, 0, (TaskFunc) r222_DummyDoorProc, 0, 1);
    SceAtDataSet_exec(9, 0x12, 0, (TaskFunc) asl_wait, 0, 1);
    SceAtDataSet_exec(0xA, 0x12, 0, (TaskFunc) asl_chase, 0, 1);
    SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) toroko_go, 0, 1);
    SceAtDataSet_exec(4, 0x12, 0, (TaskFunc) toroko_go, (void*) 1, 1);
    if ((pG->flags_54 & 0x100) == 0 && pG->room_id_prev == 0x210) {
        if (pG->x4F9E == 1) {
            SceExec(0x12, (TaskFunc) toroko_ret, 0, 0, 2, 0);
        } else if (pG->x4F9E == 2) {
            SceExec(0x12, (TaskFunc) toroko_ret, 1, 0, 2, 0);
        }
    }
    if (pSUB) {
        U16And(pSUB->atari.flags, 0xEFFF);
        BitOn16(pSUB->atari.flags, 0x2000);
    }
    SceAtDataSet_exec(5, 0x12, 0, (TaskFunc) r222_dai_go, 0, 1);
    SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) r222_dai_ret, 0, 1);
    SceAtDataSet_exec(7, 0x12, 0, (TaskFunc) r222_dai_set, 0, 1);
    SceAtDataSet_exec(8, 0x12, 0, (TaskFunc) r222_dai_set, 0, 1);
}

void R210Main()
{
    if (pPL->pos.z < -20000.0f) {
        SubCharCtrl(7, 0);
        pG->flags_51BC |= 0x80;
    }
}

// Area 0: the door back to r222 takes Ashley away.
static void r222_DummyDoorProc()
{
    if (pSUB) {
        EmMgr.destroy(pSUB);
        pG->flags_5018 &= ~0x04000000;
    }
    SceAtDataReset(0);
    SceAtExecute(0);
}

static void r222_dai_set()
{
    SceAtSetEnable(5, 1);
    SceAtSetEnable(6, 1);
    if ((int) pG->flags_174 < 0) {
        BitOff(pG->flags_174, 0x80000000);
        pG->flags_51BC &= ~0x80;
    }
}

// Ashley's jump onto the lift (SetSubAux routine).
static void funcAshley2(cEm* p)
{
    if (p->xFE == 0) {
        cAtariInfo* at = &pSUB->atari;

        at->throughOn();
        p->motionSet(ROOM_ARC_PTR(pGS->pRoomArc, 0x27), 0x2D, 0x2D, 1, 0);
        p->xFE = 1;
        p->motSpeedRate = 0.2f;
    }
    if (p->motionMove()) {
        cAtariInfo* at;

        // Reference store: the `lwz pSUB` (fixed scalar) below depends on an unflagged MEM store but
        // not on an in-struct one, which is what ranks the 1.0 chain above the routine bytes.
        FSet(p->motSpeedRate, 1.0f);
        EmRoutineSet(p, 0, 0, 0, 0);
        at = &pSUB->atari;
        at->throughOff();
        SubCharCtrl(1, 0);
    }
}

// Area 5: the lift goes down.
static void r222_dai_go()
{
    f32 dist = 2244.0f;
    f32 spd = 0.0f;
    Vec v;

    SceEventStart(0);
    pPL->setNoSuspend(1);
    if (pSUB) {
        cAtariInfo* at;

        pSUB->setNoSuspend(1);
        at = &pSUB->atari;
        at->throughOff();
        SubCharCtrl(7, 0);
        pG->flags_51BC |= 0x80;
    }
    if ((pG->flags_51C0 & 0x40) == 0 && CheckDoorJumpWithAshley() == 1) {
        CamCtrl.CutCall(5);
        SetPlDamage(0, plemRide);
        pPL->setNoSuspend(1);
        {
            // `cModel* m = pPL` at the block top: the pointer load leaves the store block and the two pool
            // constants' local-alloc order gives x f0 / z f13 (the st4_0 slide_move lever).
            cModel* m = pPL;

            v.x = -154.0f;
            v.z = -17194.0f;
            v.y = 0.0f;
            m->setPos(&v);
        }
        v.y = PI;
        v.x = 0.0f;
        v.z = 0.0f;
        pPL->setAng(&v);
        if (pSUB) {
            v.x = -549.0f;
            v.z = -14489.0f;
            v.y = 0.0f;
            pSUB->setPos(&v);
            v.x = 0.0f;
            v.y = 2.72f;
            v.z = 0.0f;
            pSUB->setAng(&v);
        }
        SetSubAux((int) funcAshley2, 0);
        SceSleep(10);
        SndCall(6, 5, &pPL->pos, 0, 0, 0);
        SceSleep(45);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        pSUB->motSpeedRate = 1.0f;
    }
    {
        cModel* m = pPL;

        v.x = -154.0f;
        v.z = -17194.0f;
        v.y = 0.0f;
        m->setPos(&v);
    }
    v.y = PI;
    v.x = 0.0f;
    v.z = 0.0f;
    pPL->setAng(&v);
    {
        cAtariInfo* at = &pPL->atari;

        at->throughOn();
    }
    SceAtSetEnable(5, 0);
    SceAtSetEnable(6, 0);
    SmdGetObjPtr(0x20)->be_flag |= 0x20;
    SmdGetObjPtr(0x21)->be_flag |= 0x20;
    CamCtrl.CutCall(1);
    SndCall(6, 1, &SmdGetObjPtr(0x21)->pos, 0, 0x80000000, 0);
    v.x = 0.0f;  // x, z, y: the original issues y, z, x (the shared 0.0 register's stores bracket the z store)
    v.z = -17270.0f;
    v.y = 0.0f;
    pPL->setPos(&v);
    while (SmdGetObjPtr(0x21)->pos.z > r210_daiZGo) {
        f32 dz;

        spd += (r210_daiRotGo - spd) * 0.1f;
        dz = spd * dist;
        SmdGetObjPtr(0x20)->rot.x += spd;
        SmdGetObjPtr(0x21)->pos.z += dz;
        FAdd(SmdGetObjPtr(0x20)->pos.z, dz);
        pPL->pos.z += dz;
        SceSleep(1);
    }
    SndCall(6, 2, &SmdGetObjPtr(0x21)->pos, 0, 0x80000000, 0);
    {
        cAtariInfo* at = &pPL->atari;

        AtariOnRaw(at, 0x300);
    }
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 6: the lift comes back up.
static void r222_dai_ret()
{
    f32 dist = 2244.0f;
    f32 spd = 0.0f;
    Vec v;

    pG->flags_174 |= 0x80000000;
    SceEventStart(0);
    pPL->setNoSuspend(1);
    if (pSUB) {
        cAtariInfo* at;

        pSUB->setNoSuspend(1);
        at = &pSUB->atari;
        at->throughOff();
        SubCharCtrl(7, 0);
        pG->flags_51BC |= 0x80;
    }
    SceAtSetEnable(5, 0);
    SceAtSetEnable(6, 0);
    SmdGetObjPtr(0x20)->be_flag |= 0x20;
    SmdGetObjPtr(0x21)->be_flag |= 0x20;
    CamCtrl.CutCall(2);
    SndCall(6, 1, &SmdGetObjPtr(0x21)->pos, 0, 0x80000000, 0);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = -31958.0f;
    pPL->setPos(&v);
    while (SmdGetObjPtr(0x21)->pos.z < r210_daiZRet) {
        f32 dz;

        spd += (r210_daiRotRet - spd) * 0.1f;
        dz = spd * dist;
        SmdGetObjPtr(0x20)->rot.x += spd;
        SmdGetObjPtr(0x21)->pos.z += dz;
        FAdd(SmdGetObjPtr(0x20)->pos.z, dz);
        pPL->pos.z += dz;
        SceSleep(1);
    }
    SndCall(6, 2, &SmdGetObjPtr(0x21)->pos, 0, 0, 0);
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Areas 3/4: the cart ride to r212 (dir 0: left cart, 1: right cart).
static inline f32 FCRef(const f32& v) { return v; }
static void toroko_go(int dir)
{
    cPlayer* pl = pPL;
    cObj* obj;
    Vec pos;

    if (CheckDoorJumpWithAshley() == 0) {
        cMes.MesSet(0x67, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1, 1, 0, 0, 4);
        return;
    }
    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoomArc, 0x22), ROOM_ARC_PTR(pG->pRoomArc, 0x23), (Vec*) &vecZero, (Vec*) &vecZero, 0x10, 1);
    SmdGetObjPtr(0x1E)->be_flag &= ~2;
    SmdGetObjPtr(0x1F)->be_flag &= ~2;
    if (dir == 0) {
        pos = r210_torokoGoPos0;
    } else {
        pos = r210_torokoGoPos1;
    }
    obj->be_flag |= 0x20;
    {
        // the 0.0 volume is loaded AFTER the be_flag store (a pool constant would float above it)
        static const f32 vol = 0.0f;
        SndStrReq(1, 0xE4, 0x80000003, 0, 0, FCRef(vol));
    }
    SceEventStart(0);
    pl->setRightHand(1);
    pl->pWep->setTrans(0, 0);
    PlSetHand(1, 0);
    SubCharCtrl(5, 0);
    {
        cPlayer* p = pPL;
        Vec* pp = &pos;
        Vec* zero = (Vec*) &vecZero;

        p->setPos(pp);
        p->setAng(zero);
        {
            cSubChar* sub = pSUB;

            if (sub) {
                sub->setPos(pp);
                sub->setAng(zero);
            }
        }
        obj->setPos(pp);
        obj->setAng(zero);
    }
    pPL->setNoSuspend(1);
    if (pSUB) {
        pSUB->setNoSuspend(1);
    }
    MotionSetCore(pPL, &pPL->mot, ROOM_ARC_PTR(pG->pRoomArc, 0x1F), 0, 0, 1, 0);
    if (pSUB) {
        MotionSetCore(pSUB, &pSUB->mot, ROOM_ARC_PTR(pG->pRoomArc, 0x20), 0, 0, 1, 0);
    }
    obj->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x21), 10, 0, 1, 0);
    SceSleep(115);
    SndCall(6, 3, &pPL->pos, 0, 0, 0);
    SndCall(6, 4, &pPL->pos, 0, 0, 0);
    SceSleep(50);
    FadeSetW(2, 15, 0, 0);
    SceSleep(15);
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    SceEventEnd(0);
    PlSetHand(0, 0);
    pl->setRightHand(1);
    pl->pWep->setTrans(1, 0);
    if (dir == 0) {
        SceAtDataReset(3);
        SceAtExecute(3);
    } else {
        SceAtDataReset(4);
        SceAtExecute(4);
    }
}

// Inline taking the angle by pointer: the caller's `&ang` becomes a hard-register argument set at each
// call (never a gcse occurrence), so it is recomputed `addi r4,r1,24` instead of sharing the template copy's
// address pseudo.
static inline void r210_setAng(cModel* m, Vec* a)
{
    m->setAng(a);
}

// The cart ride back from r212 (dir 0: left cart, 1: right cart).
static void toroko_ret(int dir)
{
    cPlayer* pl = pPL;
    cObj* obj;
    Vec pos;

    if (CheckDoorJumpWithAshley() == 0) {
        cMes.MesSet(0x67, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1, 1, 0, 0, 4);
        return;
    }
    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoomArc, 0x22), ROOM_ARC_PTR(pG->pRoomArc, 0x23), (Vec*) &vecZero, (Vec*) &vecZero, 0x10, 1);
    Vec ang = {0.0f, -0.07f, 0.0f};
    SmdGetObjPtr(0x1E)->be_flag &= ~2;
    SmdGetObjPtr(0x1F)->be_flag &= ~2;
    if (dir == 0) {
        pos = r210_torokoRetPos0;
    } else {
        pos = r210_torokoRetPos1;
    }
    obj->be_flag |= 0x20;
    SceEventStart(0);
    SndStrReq(1, 0xE5, 0x80000003, 0, 0, 0.0f);
    pl->setRightHand(1);
    pl->pWep->setTrans(0, 0);
    PlSetHand(1, 0);
    SceSleep(1);
    SubCharCtrl(5, 0);
    {
        cPlayer* p = pPL;
        Vec* pp = &pos;

        p->setPos(pp);
        r210_setAng(p, &ang);
        {
            cSubChar* sub = pSUB;

            if (sub) {
                sub->setPos(pp);
                r210_setAng(sub, &ang);
            }
        }
        obj->setPos(pp);
        r210_setAng(obj, &ang);
    }
    pPL->setNoSuspend(1);
    if (pSUB) {
        pSUB->setNoSuspend(1);
    }
    MotionSetCore(pPL, &pPL->mot, ROOM_ARC_PTR(pG->pRoomArc, 0x24), 0, 0, 1, 0);
    if (pSUB) {
        MotionSetCore(pSUB, &pSUB->mot, ROOM_ARC_PTR(pG->pRoomArc, 0x25), 0, 0, 1, 0);
    }
    obj->motionSet(ROOM_ARC_PTR(pG->pRoomArc, 0x26), 10, 0, 1, 0);
    SceSleep(20);
    SndCall(6, 3, &pPL->pos, 0, 0, 0);
    SndCall(6, 4, &pPL->pos, 0, 0, 0);
    SceSleep(140);
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    SubCharCtrl(1, 1);
    ObjMgr.destroy(obj);
    SmdGetObjPtr(0x1E)->be_flag |= 2;
    SmdGetObjPtr(0x1F)->be_flag |= 2;
    SceEventEnd(0);
    PlSetHand(0, 0);
    pl->setRightHand(1);
    pl->pWep->setTrans(1, 0);
    if (dir == 0) {
        Vec p;
        Vec* pp = &p;
        f32 y;

        {
            cPlayer* p1 = pPL;

            p.x = -19403.0f;
            y = -2000.0f;
            pp->y = y;
            pp->z = -8345.0f;
            p1->setPos(pp);
        }
        {
            cPlayer* p1 = pPL;

            p.x = 0.0f;
            pp->y = 1.7f;
            p.z = 0.0f;
            p1->setAng(pp);
        }
        if (pSUB) {
            p.x = -19697.0f;
            pp->y = y;
            pp->z = -7950.0f;
            pSUB->setPos(pp);
            {
                cSubChar* s = pSUB;

                p.x = 0.0f;
                pp->y = 1.75f;
                p.z = 0.0f;
                s->setAng(pp);
            }
        }
    } else {
        Vec p;
        Vec* pp = &p;
        f32 y;

        {
            cPlayer* p1 = pPL;

            p.x = -61000.0f;
            y = -2000.0f;
            pp->y = y;
            pp->z = 246652.0f;
            p1->setPos(pp);
        }
        {
            cPlayer* p1 = pPL;

            p.x = 0.0f;
            pp->y = 1.67f;
            p.z = 0.0f;
            p1->setAng(pp);
        }
        if (pSUB) {
            p.x = -61850.0f;
            pp->y = y;
            pp->z = 247150.0f;
            pSUB->setPos(pp);
            {
                cSubChar* s = pSUB;

                p.x = 0.0f;
                pp->y = 1.83f;
                p.z = 0.0f;
                s->setAng(pp);
            }
        }
    }
}

// Leon's ride motion on the lift (SetPlDamage routine).
static void plemRide(cPlayer* pl)
{
    switch (pl->xFE) {
    case 0:
        pPL->setNoSuspend(1);
        MotionSetCore(pPL, &pPL->mot, pl->pMotTbl[11], 0, 0, 0x201, 0);
        pl->xFE++;
        pl->xFF = 0;
    case 1:
        pl->xFF++;
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
}

// The module's .data continues 8-aligned.
asm(".section .data; .balign 8");
