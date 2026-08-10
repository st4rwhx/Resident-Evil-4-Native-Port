#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "esp.h"
#include "player.h"
#include "item.h"
#include "mes.h"
#include "cam_ctrl.h"
#include "cockpit.h"
#include "sscrn.h"
#include "snd.h"
#include "flr_at.h"
#include "rnd.h"

// Room 1-18 (D:/Bio4/Prog/r118.cpp): the church in the storm; the door 117 key, the altar view,
// r108's symbol puzzle, Ashley's call and the thunder task with the dog.

struct R118Work {
    u8 bgmLow;     // 0x00  BGM lowered while the player stands in area 7
    u8 pad_1[3];
    cEmWrap em;    // 0x04  the dog (list entry 0x78)
    u32 strId;     // 0x10  SndStrReq handle of the altar view stream
};

static R118Work* r118_work;

extern "C" void r108_initPuzzle(int dial, int coverL, int coverR, int mesNo);

static void r118_execShowView_end();
static void r118_execShowView();
static void r118_execAshleyVoice();
static void r118_checkBgm();
static void r118_checkDoor117KeyUse();
static void r118_checkDoor117();
static void r118_ThunderFlagOn();
static void r118_ThunderFlagOff();
static void r118_ThunderMove();

void R118Init()
{
    cModel* m;
    int zero = 0;

    pG->flags_54 &= ~0x800;
#line 47 "D:/Bio4/Prog/r118.cpp"
    r118_work = (R118Work*) MEM_CALLOC(sizeof(R118Work), 1, 0xd);

    SmdGetObjPtr(0)->be_flag &= ~2;
    SmdGetObjPtr(0)->lightInfo.x50 = zero;
    SceExec(0x12, (TaskFunc) r118_ThunderMove, 0, 0, 2, 0);
    EstSet((int) pPL, -1, 0, 0, 3, 2, 0x800, 0, 0, 0);
    EstSet((int) pPL, -1, 0, 0, 1, 5, 0x800, 0, 0, 0);
    pG->flags_5010 |= 0x400;
    SceAtSetEnable(0x80, 1);
    if ((m = SceAtItemModelPtr(0x80)) != 0) {
        m->lightInfo.x50 = (m->lightInfo.x50 & ~0x20) | 0x10;
        m->setNoSuspend(1);
    }
    if (!(pG->door_unlock[0] & 0x10000000)) {
        SceAtSetEnable(0x80, 0);
        SceAtDataSet_exec(4, 0x12, 0, (TaskFunc) r118_checkDoor117, 0, 1);
        SceExec(0x12, (TaskFunc) r118_checkDoor117KeyUse, 0, 0, 2, 0);
    }
    if (pG->flags_51BC & 0x00100000) {
        EM_LIST(0x82)->flags &= ~1;
        EM_LIST(0x83)->flags &= ~1;
        EM_LIST(0x84)->flags &= ~1;
        EmSetFromList2(0x79, 1);
        EmSetFromList2(0x7A, 1);
        EmSetFromList2(0x7B, 1);
        EmSetFromList2(0x7C, 1);
        EmSetFromList2(0x7D, 1);
        EmSetFromList2(0x7E, 1);
        EmSetFromList2(0x7F, 1);
        SndRoomStrStart(1, 5, 1);
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            SceExec(0x12, (TaskFunc) r118_execShowView, 0, 0, 2, 0);
        }
        EspDataLoad((u32) ROOM_ARC_PTR(pG->pRoomArc, 0x1F), 0xCA, 0);
        r118_work->em.setEm(0x78, -1, 0, 1, 1);
        SceAtDataSet_exec(9, 0x12, 0, (TaskFunc) r118_execAshleyVoice, 0, 1);
        SceAtSetEnable(1, 0);
        SmdSetTrans(0x26, 0);
    } else {
        SmdSetTrans(0x1B, 0);
        SceAtSetEnable(8, 0);
        SceAtSetEnable(2, 0);
    }
    SceExec(0x12, (TaskFunc) r118_checkBgm, 0, 0, 2, 0);
    r108_initPuzzle(0x31, 0x32, 0x33, 2);
    FlrAtSetDefVal(0, 0, 3);
}

void R118Main()
{
}

static void r118_execShowView_end()
{
    SndStrReq(r118_work->strId, 4, 50, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Show the altar: camera cut 10 with its stream.
// OPEN (as r108 execShowView): the original issues the stream's `lfs f1, 0.0` after the RsfSet store.
static inline f32 FCRef(const f32& v) { return v; }

static void r118_execShowView()
{
    // The 0.0 is loaded after the RsfSet store: a pool constant would move above it (pool loads never
    // depend on stores), a `static const` read through a reference stays below (docs/matching.md, cSceObj).
    static const f32 vol = 0.0f;

    RsfSet(G_ROOM_ID, 0);
    r118_work->strId = SndStrReq(1, 0xE0, 0x80000003, 0, 0, FCRef(vol));
    SceSetEventCancel(1, (TaskFunc) r118_execShowView_end, 0, -1, 1);
    SceEventStart(0);
    pG->flags_5010 &= ~0x10000000;
    CamCtrl.CutCall(0xA);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r118_execShowView_end();
}

// Ashley calls out while the dog is still alive.
static void r118_execAshleyVoice()
{
    if (CheckAshleyActive()) {
        cEmWrap em;

        em.setPtr(0x78, -1, 0);
        if (em.isActive() == 1) {
            int i;
            MessageControl* mes;

            SceAtSetEnable(9, 0);
            i = 0;
            SndCall(6, 0, 0, 0, 0, 0);
            mes = &cMes;
            SceMesSet(8, 0xB0, 1, 0x64, 0x150 - mes->getWork()->lineSpace - mes->getWork()->fontH - 1);
            SceSleep(90);
            for (; i < 16; i++) {
                mes->Delete(i);
            }
            Cckpt.lifeMeterDisp(1);
        }
    }
}

// Room BGM: track 1 lowered while the player stands in area 7.
static void r118_checkBgm()
{
    SceSleep(1);
    if (pG->room_id_prev == 0x119) {
        r118_work->bgmLow = 1;
        SndRoomBgmStart(1, 0);
    } else {
        r118_work->bgmLow = 0;
        SndRoomBgmStart(1, 1);
    }
    SceSleep(1);
    for (;;) {
        int hit = SceAtHitCheck(7);

        if (hit == 1) {
            if (r118_work->bgmLow == 0) {
                r118_work->bgmLow = hit;
                SndRoomBgmVolReset(1, 2000);
            }
        } else {
            if (r118_work->bgmLow == 1) {
                r118_work->bgmLow = 0;
                SndRoomBgmVolSet(1, 1, 2000);
            }
        }
        SceSleep(1);
    }
}

// Once the player holds the key: camera cut 9, the door unlocks.
static void r118_checkDoor117KeyUse()
{
    while (ItemMgr.check(0x3C) != 1) {
        SceSleep(1);
    }
    SceEventStart(0);
    CamCtrl.CutCall(9);
    SceSleep(20);
    SceAtSetEnable(0x80, 1);
    SndCall(6, 8, 0, 0, 0, 0);
    SceMesSet(1, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
    pG->door_unlock[0] |= 0x10000000;
    SceAtDataReset(4);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The locked door: the up-cut message, then the sub screen terminal / the key use.
static void r118_checkDoor117()
{
    SceUpCut(0, 9, 7, 4);
    if (ItemMgr.num(0x3C) == 0) {
        CamCtrl.Comeback(0);
        if (!(pG->flags_51C0 & 0x00080000)) {
            pG->flags_51C0 |= 0x00080000;
            OpeSetOpenTerm(7, 22600.0f, 11775.0f, -26200.0f, 1.6f);
        }
    } else {
        SubScreenOpen(0x80, 1);
    }
}

// Lightning: the lit window object on with the bright sky colour.
static void r118_ThunderFlagOn()
{
    SmdGetObjPtr(0)->be_flag |= 2;
    SmdGetObjPtr(0x64)->pInfo->color[0] = 0xEB;
    SmdGetObjPtr(0x64)->pInfo->color[1] = 0xF9;
    SmdGetObjPtr(0x64)->pInfo->color[2] = 0xFF;
}

static void r118_ThunderFlagOff()
{
    SmdGetObjPtr(0)->be_flag &= ~2;
    SmdGetObjPtr(0x64)->pInfo->color[0] = 0x5F;
    SmdGetObjPtr(0x64)->pInfo->color[1] = 0x61;
    SmdGetObjPtr(0x64)->pInfo->color[2] = 0x67;
}

// Thunder every 90..235 frames (30..117 while the player is in area 4); no flash while the dog
// has found the player.
static void r118_ThunderMove()
{
    int cnt;
    cEm* em;

    SceSleep(1);
    EffSetToolStateCallBack(0, r118_ThunderFlagOn, r118_ThunderFlagOff);
    {
        u8 r = Rnd() % 30;
        cnt = r * 5 + 90;
    }
    em = r118_work->em.getPtr();
    for (;;) {
        if (cnt <= 0) {
            // COMPILER-DIFF: dead test; its 0x88888889 constant is loop.c's FIRST movable, so the
            // hoisted `lis/ori` pair precedes the then arm's zero in the preheader (target LUID order).
            if (cnt == (int) 0x88888889) cnt = 0;
            if (EffGetAreaState(3) == 0 && (em == 0 || ((cEmDog*) em)->ckFindPL() != 1)) {
                if (Rnd() & 0x80) {
                    // COMPILER-DIFF: candidate #12 (fallthrough-arm form): a plain zero here is folded by cse1 into
                    // the EffGetAreaState result (path re-walk); the code-less `do {} while (0)` ends cse's path
                    // at its NOTE_INSN_LOOP_END, so the zero stays a constant pseudo that loop.c hoists (r28).
                    void* zero;
                    do { } while (0);
                    zero = 0;
                    EstSet(0, -1, 0, 0, 1, 4, 1, 0, (u32) zero, zero);
                } else {
                    EstSet(0, -1, 0, 0, 1, 1, 1, 0, 0, 0);
                }
                if (EffGetAreaState(4) != 0) {
                    u8 r = Rnd() % 30;
                    cnt = r * 3 + 30;
                } else {
                    u8 r = Rnd() % 30;
                    cnt = r * 5 + 90;
                }
            } else {
                u8 r = Rnd() % 30;
                cnt = r * 5 + 90;
            }
            SceSndCallThunder();
        }
        cnt--;
        SceSleep(1);
    }
}
