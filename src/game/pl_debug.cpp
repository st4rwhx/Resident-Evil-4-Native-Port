// game/pl_debug.cpp: player debug helpers: cheat ("maho") commands, collision test draws, gauge.

#include "player.h"
#include "global.h"
#include "atari.h"
#include "em.h"
#include "joy.h"
#include "db_log.h"
#include "eprintf.h"
#include "dbmodule.h"
#include "math_sub.h"

extern cModel* pSUB;

int MotionSetCore(cModel* m, void* work, void* data, int a, int b, int c, int d);  // game/motion.cpp
extern "C" void EmYarareDisp(cModel* m);                                            // game/em_sub.cpp
extern "C" void DrawOba(cModel* m);                                                 // game/at_mod.cpp

int scr_hit_check = 0;
static int sat_make_test = 0;
int local_coord_test = 0;

u8 PlCapNum[25];

void mahoMuteki()
{
    BitOn(pG->flags_68, 0x800000);
    pLog->mes(0, 0, "NO DEATH ON");
}

void mahoInfBul()
{
    BitOn(pG->flags_68, 0x400000);
    pLog->mes(0, 0, "INF BULLET ON");
}

void mahoSkelOn()
{
    BitOn(pG->flags_60, 0x8000000);
    BitOn(pG->flags_58, 0x8000000);
    pLog->mes(0, 0, "SKELTON ON");
}

void mahoSkelOff()
{
    BitOff(pG->flags_60, 0x8000000);
    BitOff(pG->flags_58, 0x8000000);
    pLog->mes(0, 0, "SKELTON OFF");
}

void mahoCallSc()
{
    if (pSUB) {
        pSUB->setPos(&pPL->pos);
        pLog->mes(0, 0, "ASHLEY TELEPORT");
    }
}

static void mahoKaiouOff()
{
    BitOff(pG->flags_68, 0x10000);
    pLog->mes(0, 0, "KAIOUKEN OFF");
}

void mahoKaiou2()
{
    BitOn(pG->flags_68, 0x10000);
    PlKaiou = 0;
    pLog->mes(0, 0, "KAIOUKEN x2");
}

void mahoKaiou3()
{
    BitOn(pG->flags_68, 0x10000);
    PlKaiou = 1;
    pLog->mes(0, 0, "KAIOUKEN x3");
}

void mahoKaiou4()
{
    BitOn(pG->flags_68, 0x10000);
    PlKaiou = 2;
    pLog->mes(0, 0, "KAIOUKEN x4");
}

void mahoThroughOn()
{
    pPL->atari.throughOn();
    pLog->mes(0, 0, "PL THROUGH ON");
}

void mahoThroughOff()
{
    pPL->atari.throughOff();
    pLog->mes(0, 0, "PL THROUGH OFF");
}

void cPlayer::debugInit()
{
    pMaho = new cPlMaho;
    pMaho->regist("43243262A", mahoSkelOn);
    pMaho->regist("43243262B", mahoSkelOff);
    pMaho->regist("456456A", mahoThroughOn);
    pMaho->regist("456456B", mahoThroughOff);
    pMaho->regist("40404040A", mahoKaiou4);
    pMaho->regist("404040A", mahoKaiou3);
    pMaho->regist("4040A", mahoKaiou2);
    pMaho->regist("4040B", mahoKaiouOff);
    pMaho->regist("0426A", mahoMuteki);
    pMaho->regist("0462A", mahoInfBul);
    pMaho->regist("243A", mahoCallSc);
}

void scrHitCheck(cPlayer* pl)
{
    if (scr_hit_check) {
        static u32 hcFlag = 0x8000;
        static u32 hcMask = 0;
        Vec top;
        Vec dir;
        Vec hit;
        Vec nrm;
        int ret;

        top = pl->pos;
        top.y += 1000.0f;
        dir.x = 0.0f;
        dir.y = 0.0f;
        dir.z = 5000.0f;
        PSMTXMultVecSR(pl->mat, &dir, &dir);
        PSVECAdd(&dir, &top, &dir);
        ret = SatMgr.hitCheck(&top, &dir, &hit, &nrm, hcFlag, hcMask);
        Draw_line3d(&top, &hit, ret ? 0xFFFF0000 : 0xFFFFFFFF, 0);
        if (ret) {
            PSVECScale(&nrm, &dir, 1000.0f);
            PSVECAdd(&dir, &hit, &dir);
            Draw_line3d(&hit, &dir, 0xFFFFFFFF, 0);
        }
    }
}

void satMakeTest(cPlayer* pl)
{
    if (sat_make_test) {
        static cSat* pS0 = 0;
        static Vec quad[4] = {
            {-1000.0f, 0.0f, -1000.0f},
            {1000.0f, 0.0f, -1000.0f},
            {1000.0f, 0.0f, 1000.0f},
            {-1000.0f, 0.0f, 1000.0f},
        };
        static Vec z0 = {0.0f, 0.0f, 1000.0f};
        Vec pos;
        cSatMgr* sat = &SatMgr;

        eprintf(100, 100, 0, 0, "%d", sat->nArray);
        if (Joy[0].on & JOY_A) {
            if (pS0) {
                SatMgr.destroy(pS0);   // on the object: devirtualised `bl destroy__7cSatMgrP4cSat`
                pS0 = 0;
            }
            RotVector(&z0, &pl->rot, &pos);
            PSVECAdd(&pos, &pl->pos, &pos);
            pS0 = sat->create(&pos, &pl->rot, quad, 0, 0x200, 0.0f);
        }
    }
}

void localCoordTest(cPlayer* pl)
{
    if (local_coord_test) {
        static Vec vpos;
        static u16 pl_db_parts_no = 0;
        Mtx m;

        vpos.x += (f32) Joy[2].sx / 100.0f;
        vpos.y += (f32) Joy[2].sy / 100.0f;
        vpos.z = vpos.z + (f32) Joy[2].trigR / 200.0f - (f32) Joy[2].trigL / 200.0f;
        if (Joy[0].trg & JOY_Y) {
            pl_db_parts_no++;
        }
        if (Joy[0].trg & JOY_X) {
            pl_db_parts_no--;
        }
        PSMTXConcat(pG->Cam.viewMat, pl->getPartsPtr(pl_db_parts_no)->mat, m);
        Draw_local_pos(&vpos, 1000, m);
        eprintf(40, 100, 0, 0, "%5.2f", vpos.x);
        eprintf(40, 116, 0, 0, "%5.2f", vpos.y);
        eprintf(40, 132, 0, 0, "%5.2f", vpos.z);
    }
}

// Dead-stripped by the original linker (pool "%f", 10000.0f and the static `tang` remain).
static void tangentTest(cPlayer* pl)
{
    static Vec tang;

    tang = pl->speed;
    eprintf(40, 100, 0, 0, "%f", tang.x * 10000.0f);
}

void cPlayer::debugMove()
{
    scrHitCheck(this);
    satMakeTest(this);
    emSearch();
    localCoordTest(this);
    EmYarareDisp(this);
    if (pG->flags_68 & 0x10000000) {
        DrawOba(this);
    }
    if (PlDbFlag & 2) {
        Draw_pos(&pos, 1000);
    }
}

void cPlayer::emSearch()
{
    f32 min = 100000.0f;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((em->be_flag & 0x201) == 1 && em->hp > 0) {
            f32 d = GetDistance3(&pos, &em->pos);
            if (d > min) {
                continue;
            }
            min = d;
        }
    }
}

void PlWepMotSet(int no)
{
    void* mot = 0;
    cPlayer* pl = pPL;

    switch (no) {
    case 0:
        mot = PlWepMot[0];
        break;
    case 1:
        mot = PlWepMot[1];
        break;
    case 2:
        mot = PlWepMot[2];
        break;
    case 3:
        mot = pl->pMotTbl[0];
        break;
    }
    MotionSetCore(pl, &pl->pMotion, mot, 0, 3, 5, 0);
}

cPlMaho::cPlMaho()
{
    reset();
    num = 0;
}

void cPlMaho::reset()
{
    int i;

    for (i = 0; i < 30; i++) {
        tbl[i].x0 = 0;
    }
}

void cPlMaho::regist(const char* code, void (*func)())
{
    PlMahoEntry* e = &tbl[num];

    e->x0 = 0;
    e->x1 = 0;
    e->func = func;
    e->code = code;
    num++;
}

// DrawGage: `len` is the one variable for both widths and the right edge (a multi-set pseudo is
// never tied to the dying product, so `fw*fnow` stays in f12 like the original); the original's
// `fadds f25,f13,f25` (fx first) comes from adding a cse-deleted copy of len (see the note there).
void DrawGage(int x, int y, int h, int w, int now, int max, int color)
{
    Vec pos;
    Vec size;
    f32 len, fx, fy, fw, fnow, fmax, fh;

    if (now > max) {
        now = max;
    }
    fx = (f32) x;
    fy = (f32) y;
    fw = (f32) w;
    fnow = (f32) now;
    fmax = (f32) max;
    fh = (f32) h;
    len = fw * fnow / fmax;

    pos.x = fx;
    pos.y = fy;
    pos.z = 1.0f;
    size.x = len;
    size.y = fh;
    size.z = 1.0f;
    // `len = fx + len` is expanded with the destination operand first (`fadds len,len,fx`:
    // expand_binop swaps a commutative op when op1 is the target rtx); the original has `fadds
    // len,fx,len`. Adding a copy of len keeps op1 != target, cse canonicalises the copy back to
    // len (deleting it) and leaves the operand order alone.
    f32 l2 = len;
    len = fx + l2;
    Draw_quad(&pos, &size, color);

    pos.x = len;
    pos.y = fy;
    pos.z = 1.0f;
    len = fw * (1.0f - fnow / fmax);
    size.x = len;
    size.y = fh;
    size.z = 1.0f;
    Draw_quad(&pos, &size, color & 0xFF000000);
}

// the split object's .sdata is 8-aligned
asm(".section .sdata; .balign 8");
