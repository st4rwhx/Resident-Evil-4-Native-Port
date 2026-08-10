// game/mercenaries: the Mercenaries minigame (score, combo, bonus time, result screens)
// (D:/Bio4/Prog/mercenaries.cpp).
#include "types.h"
#include "global.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "sofdec.h"
#include "id_sys.h"
#include "mes.h"
#include "main.h"
#include "main_mem.h"
#include "snd.h"
#include "cockpit.h"
#include "sce.h"
#include "sce_sys.h"
#include "game.h"
#include "player.h"
#include "obj.h"
#include "scroll.h"
#include "motion.h"
#include "cam_ctrl.h"
#include "fade.h"
#include "dvd.h"
#include "cDataSwap.h"
#include "option.h"
#include "mercenaries.h"

extern "C" {
void* memset(void* dst, int c, unsigned int n);
static void IdSetColLoop(IDSystem* id, int no, u8 type, int on);
}

// The original cUnit::beginEvent/endEvent take an int; the shared cUnit declaration still has
// the no-argument form, so the player calls go through this view of the vtable (sscrn.cpp).
class cUnitEvent {
public:
    u32 be_flag;
    cUnit* next;
    virtual ~cUnitEvent();
    virtual void beginEvent(int mode);
    virtual void endEvent(int mode);
};
#define BEGIN_EVENT(p, mode) ((cUnitEvent*) (p))->beginEvent(mode)
#define END_EVENT(p, mode) ((cUnitEvent*) (p))->endEvent(mode)

#define ARC_PTR(ofs) ((void*) (pG->pArc->ofs + (u32) pG->pArc))
#define DATA_PTR(d, ofs) ((void*) (*(u32*) ((u8*) (d) + (ofs)) + (u32) (d)))
#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// Message y: below the bottom line of the message window.
#define MES_Y(m) (336 - (m)->lineSpace - (m)->fontH - 1)

#define ID_MERC 0x22
#define ID_MERC_MES 0x2C
#define ID_RESULT 0x28

#define KEY_A 0x80000000

// Combo counter shown (bit 24) / hiding (bit 23), bonus time shown / hiding (22 / 21),
// bonus points shown (20), time warning colour (19), time added (18), all ranks S (25),
// per-stage record unlock (26..29, mercSysGetFlag), 31: cleared at room start.
#define MF_COMBO_ON 0x01000000
#define MF_COMBO_OFF 0x00800000
#define MF_BONUS_ON 0x00400000
#define MF_BONUS_OFF 0x00200000
#define MF_BONUS_SCORE 0x00100000
#define MF_TIME_WARN 0x00080000
#define MF_ADD_TIME 0x00040000
#define MF_ALL_RANK 0x02000000

// Bit `no` of the u32 array `tbl`, MSB first (pSys->x4 / pSys->x20 / MercSysWork::flags).
static inline u32 flagCk(u32* tbl, u32 no)
{
    return tbl[no >> 5] & (0x80000000 >> (no & 0x1F));
}

static inline void flagOn(u32* tbl, u32 no)
{
    tbl[no >> 5] |= 0x80000000 >> (no & 0x1F);
}

// Reading a global through a reference keeps its load below a preceding member store.
static inline int IRef(int& v)
{
    return v;
}

static inline SystemWork* SysRef(SystemWork*& p)
{
    return p;
}

// Through a pointer parameter: `&Fade[2]` stays a loop-invariant pseudo (`addi rX, Fade+0x48@l`).
static inline int fadeIsOn(FadeWork* f)
{
    return f->flags & 1;
}

#define SYS_FLAG_TBL ((u32*) &pSys->x4)

// Struct-member view of pSys (the pLog trick): its load stays below preceding stores through `wk`.
struct SystemWorkPtr {
    SystemWork* p;
};
#define pSysS (((SystemWorkPtr*) &pSys)->p)
#define SYS_FLAG_TBL_S ((u32*) &pSysS->x4)
#define MID (&mercId.idsys)

MercSysWork MercSysWk;
MercID mercId;

static int MercMin = 2;
int MercSec = 0;
int MercCes = 0;
static u32 BonusTimeAdd = 1000;
int ComboTimerMax = 300;
int ComboTimerFlash = 120;
static int BonusTimerFlash = 120;

// bit of MercSysWork::flags set when the stage record is unlocked
u32 mercSysGetFlag[4] = {2, 3, 4, 5};
// pSys->x4 bit per stage: extra content unlocked
u32 extFlagTbl[4] = {4, 6, 5, 7};
// score thresholds per stage and rank
u32 RankTbl[4][6] = {
    {0, 1, 10000, 20000, 30000, 60000},
    {0, 1, 10000, 20000, 30000, 60000},
    {0, 1, 10000, 20000, 30000, 60000},
    {0, 1, 10000, 20000, 30000, 60000},
};

// points per kill by enemy kind and combo count (combo 1..9, 10+)
const u32 addScoreTbl[10][10] = {
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {5000, 5500, 6000, 6500, 7000, 7500, 8000, 8500, 9000, 9500},
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {7000, 7500, 8000, 8500, 9000, 9500, 10000, 10500, 11000, 11500},
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {10000, 10500, 11000, 11500, 12000, 12500, 13000, 13500, 14000, 14500},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
// base points per enemy kind
const u32 defaultScoreTbl[10] = {300, 300, 5000, 300, 300, 7000, 300, 300, 10000, 0};

int MercSysInitStage()
{
    MercSysWork* wk = &MercSysWk;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    memset(wk, 0, sizeof(MercSysWork));
    return 1;
}

// The SndStrReq 0.0 pool load is issued below the table stores: the call was an inlined wrapper
// (integrate.c drops RTX_UNCHANGING_P from the pool MEM, so the load depends on the stores).
static inline u32 MercStrReq(int no)
{
    return SndStrReq(0, no, 0x80000003, 0, 0, 0.0f);
}

int MercSysInitRoom(MercInit* pMInit)
{
    MercSysWork* wk = &MercSysWk;
    cObj* smd;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    if (pMInit == NULL) {
        pLog->err(0, 0, "St4ResultInitRoom : pMInit is NULL");
        return 0;
    }
    memset(&wk->score, 0, sizeof(MercSysWork) - 0x24);
    GamePointInit(2);
    {
        // COMPILER-DIFF: #13 (int shape): the original never allocates the single-use REG_EQUIV zero;
        // reload re-materialises `li r11, 0` right before the store (after the pG load), where ours
        // local-allocs the constant to r0 and hoists it above the load.
        register int z asm("r11"); // COMPILER-DIFF: #13
        z = 0;
        wk->stage = z;
    }
    if (pG->room_id != 0x400) {
        if (pG->room_id == 0x402) {
            wk->stage = 1;
        } else if (pG->room_id == 0x403) {
            wk->stage = 2;
        } else if (pG->room_id == 0x404) {
            wk->stage = 3;
        } else {
            pLog->err(0, 0, "St4ResultInitRoom : RoomNo failed");
        }
    }
    wk->mode = 0;
    if (pG->x4FB8 == 0) {
        wk->mode = 0;
    }
    if (pG->x4FB8 == 2) {
        wk->mode = 1;
    }
    if (pG->x4FB8 == 4) {
        wk->mode = 2;
    }
    if (pG->x4FB8 == 3) {
        wk->mode = 3;
    }
    if (pG->x4FB8 == 5) {
        wk->mode = 4;
    }
    wk->x70 = pMInit->x18;
    wk->smdMot = pMInit->smdMot;
    wk->x78 = pMInit->x20;
    wk->mesStart = pMInit->mesStart;
    wk->mesA8 = pMInit->mesA8;
    wk->mesAC = pMInit->mesAC;
    wk->xB0 = pMInit->x58;
    wk->mes[0] = pMInit->mes[0];
    wk->mes[1] = pMInit->mes[1];
    wk->mes[2] = pMInit->mes[2];
    wk->mes[3] = pMInit->mes[3];
    wk->mes[4] = pMInit->mes[4];
    wk->mes[5] = pMInit->mes[5];
    wk->mes[6] = pMInit->mes[6];
    wk->mes[7] = pMInit->mes[7];
    wk->mes[8] = pMInit->mes[8];
    wk->mes[9] = pMInit->mes[9];
    smd = SetObjSmd(ARC_PTR(ofs_20), ARC_PTR(ofs_24), &pMInit->pos, &pMInit->rot, 0x10, 0);
    wk->smd = smd;
    if (smd == NULL) {
        pLog->err(0, 0, "St4ResultInitRoom : DummyModel no create");
    } else {
        smd->setNoSuspend(1);
        smd->be_flag |= 0x20;
        if (smd->p2A4 == NULL) {
#line 274 "D:/Bio4/Prog/mercenaries.cpp"
            smd->p2A4 = MEM_CALLOC(0x98, 1, 13);
        }
    }
    pPL->setPos(&pMInit->pos);
    pPL->setAng(&pMInit->rot);
    pPL->matUpdate();
    CamCtrl.Comeback(0);
    SceExec(0x12, (TaskFunc) MercSysMoveMain, (int) wk, 4, 2, 0);
    {
        int strTbl[5] = {0x3F, 0x40, 0x41, 0x42, 0x3D};

        wk->strId = MercStrReq(strTbl[wk->mode]);
    }
    wk->flags &= 0x7FFFFFFF;
    mercId.init(0x60);
    return 1;
}

int MercSysMoveStart(MercSysWork* wk)
{
    u8* st;
    cObj* smd;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    st = wk->startSt;
    smd = wk->smd;
    memset(st, 0, 5);
    SceSleep(2);
    SceEventStart(1);
    BEGIN_EVENT(pPL, 0);
    pPL->setNoSuspend(1);
    Cckpt.getCountDown()->flags |= 1;
    Cckpt.getCountDown()->initTime(MercMin, MercSec, MercCes);
    // Codeless fake store surviving to global alloc: one more real insn in the range of the
    // hoisted `mercId.idsys` high (r24) but not in `&cMes`'s (r25), so their equal-priority
    // buckets (int(10000 * floor(log2 refs) * refs / len): 290/289) tie and the lower pseudo
    // (&cMes) is allocated first, as in the original.
    asm("" : "=m"(*(u16*) st)); // COMPILER-DIFF: tie (global-alloc live length)
    Cckpt.getCountDown()->frameOut();
    st[0] = 1;
    do {
        switch (st[1]) {
        case 0:
            if (smd != NULL) {
                MotionSetCore(smd, MOTION(smd), wk->smdMot, 0, 0, 0x200, 0);
            }
            st[1]++;
            break;
        case 1: {
            MesWork* m = cMes.getWork();

            SceMesSet(wk->mesStart, 0x20, 1, 100, MES_Y(m));
            if (!flagCk(SYS_FLAG_TBL, extFlagTbl[wk->stage])) {
                SceMesSet(wk->mesA8, 0x20, 1, 100, MES_Y(m));
            } else {
                MercSaveWork save;

                MercSysGetSaveWork(&save);
                if (save.rank[wk->mode][wk->stage] <= 4) {
                    SceMesSet(wk->mesAC, 0x20, 1, 100, MES_Y(m));
                }
            }
            if (pG->x4FB8 == 4) {
                SceMesSet(wk->mes[4], 0, 1, 100, MES_Y(cMes.getWork()));
            }
            st[1]++;
            break;
        }
        case 2:
            MotionClear(smd, 0);
            ObjMgr.destroy(smd);
            CamCtrl.clearAttachCamera();
            CamCtrl.flags_2C &= ~8;
            st[0] = 0;
            break;
        }
        mercId.idsys.move();
        mercId.idsys.trans();
        SceSleep(1);
    } while (st[0] != 0);
    pPL->setNoSuspend(0);
    END_EVENT(pPL, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    return 1;
}

int MercSysMoveScore(MercSysWork* wk)
{
    int min;
    int sec;
    int cs;
    int zero;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    // combo counter
    if (wk->flags & MF_COMBO_ON) {
        wk->flags &= ~(MF_COMBO_ON | MF_COMBO_OFF);
        IdSetTrans(MID, 0x30, ID_MERC, 1);
        IdSetAnmStart(MID, 0x30, ID_MERC, 1);
        IdSetColInit(MID, 0x30, ID_MERC);
        IdSetColLoop(MID, 0x30, ID_MERC, 0);
    }
    if (!(wk->flags & MF_COMBO_OFF)) {
        if (wk->comboTimer > 0) {
            wk->comboTimer--;
            if (wk->comboTimer > IRef(ComboTimerFlash)) {
                IdSetTrans(MID, 0x30, ID_MERC, 1);
                IdSetColInit(MID, 0x30, ID_MERC);
                IdSetColLoop(MID, 0x30, ID_MERC, 0);
            }
            if (wk->comboTimer == ComboTimerFlash) {
                IdSetTrans(MID, 0x30, ID_MERC, 1);
                IdSetColStart(MID, 0x30, 0x3F, ID_MERC);
                IdSetColLoop(MID, 0x30, ID_MERC, 1);
            }
            if (wk->comboTimer == 0) {
                IdSetTrans(MID, 0x30, ID_MERC, 1);
                IdSetColStart(MID, 0x30, 0x3E, ID_MERC);
                IdSetColLoop(MID, 0x30, ID_MERC, 0);
                wk->flags |= MF_COMBO_OFF;
            }
        }
    } else {
        if (IdIsAnimEnd(MID, 0x30, ID_MERC)) {
            wk->combo = 0;
            wk->flags &= ~MF_COMBO_OFF;
        }
    }
    // bonus time
    if (wk->flags & MF_BONUS_ON) {
        wk->flags &= ~(MF_BONUS_ON | MF_BONUS_OFF);
        IdSetTrans(MID, 0x40, ID_MERC, 1);
        IdSetAnmStart(MID, 0x40, ID_MERC, 1);
        IdSetColInit(MID, 0x40, ID_MERC);
        IdSetColLoop(MID, 0x40, ID_MERC, 0);
    }
    if (!(wk->flags & MF_BONUS_OFF)) {
        if (wk->bonusTimer > 0) {
            wk->bonusTimer--;
            if (wk->bonusTimer > IRef(BonusTimerFlash)) {
                IdSetTrans(MID, 0x40, ID_MERC, 1);
                IdSetColInit(MID, 0x40, ID_MERC);
                IdSetColLoop(MID, 0x40, ID_MERC, 0);
            }
            if (wk->bonusTimer == BonusTimerFlash) {
                IdSetTrans(MID, 0x40, ID_MERC, 1);
                IdSetColStart(MID, 0x40, 0x3F, ID_MERC);
                IdSetColLoop(MID, 0x40, ID_MERC, 1);
            }
            if (wk->bonusTimer <= 1) {
                IdSetTrans(MID, 0x40, ID_MERC, 1);
                IdSetColStart(MID, 0x40, 0x3E, ID_MERC);
                IdSetColLoop(MID, 0x40, ID_MERC, 0);
                wk->flags |= MF_BONUS_OFF;
            }
        }
    } else {
        if (IdIsAnimEnd(MID, 0x40, ID_MERC)) {
            wk->bonusTimer = 0;
            wk->flags &= ~MF_BONUS_OFF;
        }
    }
    // multi kill bonus
    if (wk->killCnt > 7) {
        wk->bonusScore += 4000;
    } else if (wk->killCnt > 4) {
        wk->bonusScore += 1500;
    } else if (wk->killCnt > 2) {
        wk->bonusScore += 500;
    }
    if (!(wk->flags & MF_BONUS_SCORE)) {
        if (wk->bonusScore > 0 && wk->combo == 0 && wk->bonusTimer == 0) {
            IdSetTrans(MID, 0x60, ID_MERC, 1);
            IdSetAnmStart(MID, 0x60, ID_MERC, 1);
            IdSetNum(MID, 0x61, ID_MERC, wk->bonusScore, 9999999, 7, 0);
            wk->flags |= MF_BONUS_SCORE;
            wk->bonusDisp = wk->bonusScore;
            wk->bonusScore = 0;
        }
    } else {
        if (IdIsAnimEnd(MID, 0x60, ID_MERC)) {
            wk->flags &= ~MF_BONUS_SCORE;
            wk->score += wk->bonusDisp;
        }
    }
    // score
    IdSetTrans(MID, 0x20, ID_MERC, 1);
    IdSetNum(MID, 0x21, ID_MERC, wk->score, 9999999, 7, 0);
    // time added
    zero = 0;
    if (wk->flags & MF_ADD_TIME) {
        wk->flags &= ~MF_ADD_TIME;
        min = wk->addTime / 60;
        sec = wk->addTime % 60;
        cs = zero;
        wk->addTime = zero;
        IdSetTrans(MID, 0x10, ID_MERC, 1);
        IdSetAnmStart(MID, 0x10, ID_MERC, 1);
        IdSetNum(MID, 0x15, ID_MERC, min, 9, 1, 1);
        IdSetNum(MID, 0x13, ID_MERC, sec, 99, 2, 1);
        IdSetNum(MID, 0x11, ID_MERC, cs, 99, 2, 1);
    }
    // remaining time
    Cckpt.getCountDown()->getTime(&min, &sec, &cs);
    IdSetTrans(MID, 0, ID_MERC, 1);
    IdSetNum(MID, 5, ID_MERC, min, 99, 2, 1);
    IdSetNum(MID, 3, ID_MERC, sec, 99, 2, 1);
    IdSetNum(MID, 1, ID_MERC, cs, 99, 2, 1);
    {
        int safe = 1;

        if (min <= 0) {
            safe = sec > 29;
        }
        if (safe == 0) {
            if (!(wk->flags & MF_TIME_WARN)) {
                IdSetColStart(MID, 0, 0xFE, ID_MERC);
                IdSetColLoop(MID, 0, ID_MERC, 1);
            }
            wk->flags |= MF_TIME_WARN;
        } else {
            if (wk->flags & MF_TIME_WARN) {
                IdSetColStart(MID, 0, 0xFD, ID_MERC);
                IdSetColLoop(MID, 0, ID_MERC, 0);
            }
            wk->flags &= ~MF_TIME_WARN;
        }
    }
    IdSetNum(MID, 0x31, ID_MERC, wk->combo, 999, 3, 0);
    IdSetNum(MID, 0x51, ID_MERC, BonusTimeAdd, 9999, 4, 0);
    IdSetNum(MID, 0x41, ID_MERC, wk->bonusKill, 99, 2, 0);
    wk->killCnt = 0;
    return 1;
}

int MercSysMoveMain(MercSysWork* wk)
{
    u8* st;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    st = wk->mainSt;
    memset(st, 0, 5);
    MercSysMoveStart(wk);
    mercId.dispMissionStart();
    st[0] = 1;
    do {
        MercSysMoveScore(wk);
        if (!(pG->flags_500C & 0x00100000)) {
            CountDown* cd = Cckpt.getCountDown();
            int end = 0;

            if (cd->checkState(1)) {
                end = cd->frame == 0;
            }
            if (end == 1) {
                st[0] = 0;
                break;
            }
        }
        if (Cckpt.getCountDown()->getFrame() <= 899) {
            if (wk->sndId == 0) {
                wk->sndId = SndCall(6, 0x78, 0, 0, 0, 0);
            }
        } else {
            if (wk->sndId != 0) {
                SndCall(6, 0x7A, 0, 0, 0, 0);
                wk->sndId = 0;
            }
        }
        mercId.idsys.move();
        mercId.idsys.trans();
        SceSleep(1);
    } while (st[0] != 0);
    wk->flags &= ~(MF_COMBO_ON | MF_COMBO_OFF);
    IdSetTrans(&mercId.idsys, 0x30, ID_MERC, 0);
    int zero = 0;
    wk->flags &= ~(MF_BONUS_ON | MF_BONUS_OFF);
    IdSetTrans(&mercId.idsys, 0x40, ID_MERC, 0);
    wk->combo = zero;
    wk->comboTimer = zero;
    wk->bonusTimer = zero;
    SndCall(6, 0x7A, 0, 0, 0, 0);
    MercSysResultMove(wk);
    return 1;
}

int MercSysResultInit(MercSysWork* wk)
{
    MercSaveWork save;
    int min;
    int sec;
    int cs;
    int i;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    Cckpt.getCountDown()->getTime(&min, &sec, &cs);
    wk->rslt.time = min * 6000 + sec * 100 + cs;
    wk->rslt.maxCombo = wk->maxCombo;
    wk->rslt.kill = wk->kill;
    wk->rslt.mode = wk->mode;
    wk->rslt.rank = 0;
    wk->rslt.score = wk->score;
    for (i = 0;; i++) {
        if (i < 6 && RankTbl[wk->stage][i] <= wk->rslt.score) {
            wk->rslt.rank = i;
        } else {
            break;
        }
    }
    if (wk->rslt.rank > 5) {
        pLog->err(0, 0, "MercSysResult : RankId error %d", wk->rslt.rank);
        wk->rslt.rank = 5;
    }
    MercSysGetSaveWork(&save);
    if (save.stage[wk->stage].score < wk->rslt.score) {
        save.stage[wk->stage].score = wk->rslt.score;
        save.stage[wk->stage].mode = wk->rslt.mode;
        save.stage[wk->stage].newFlag = 1;
    } else {
        save.stage[wk->stage].newFlag = 0;
    }
    if (save.rank[wk->rslt.mode][wk->stage] < wk->rslt.rank) {
        save.rank[wk->rslt.mode][wk->stage] = wk->rslt.rank;
    }
    MercSysSetSaveWork(&save);
    wk->rslt.hiScore = save.stage[wk->stage].score;
    wk->rslt.hiMode = save.stage[wk->stage].mode;
    wk->rslt.newRecord = save.stage[wk->stage].newFlag;
    if (!flagCk(SYS_FLAG_TBL_S, extFlagTbl[wk->stage])) {
        if (wk->rslt.rank > 3) {
            flagOn(SYS_FLAG_TBL_S, extFlagTbl[wk->stage]);
            flagOn(&wk->flags, mercSysGetFlag[wk->stage]);
        }
    }
    {
        int cnt = 0;
        int k;
        int j;

        for (k = 0; k < 4; k++) {
            for (j = 0; j < 5; j++) {
                if (save.rank[j][k] > 4) {
                    cnt++;
                }
            }
        }
        if (!(pSys->x4 & 0x20000000) && cnt > 19) {
            pSys->x4 |= 0x20000000;
            wk->flags |= MF_ALL_RANK;
        }
    }
    return 1;
}

int MercSysResultMove(MercSysWork* wk)
{
    static u32 stop_bak;
    static u32 disp_bak;
    static u32 MARGIN = 0x20000;
    static char data_name[] = "SS/___/omk_r1.dat";
    MercResult* pRslt;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    {
        MercRsltSt* rs = &wk->rsltSt;
        u32 size;

        memset(rs, 0, sizeof(MercRsltSt));
        rs->x8 = 0;
        SceEventStart(0);
        // constructed here: the 0x18-byte swap work is the first frame slot, the fade colours
        // and `size` only get theirs when their address is first taken
        cDataSwap swap;
        rs->run = 1;
        do {
            switch (rs->step) {
            case 0:
                mercId.dispTimeUp();
                SndRoomStrStop(3);
                SndRoomBgmStop(0, 3);
                SndStrReq(wk->strId, 4, 600, 0);
                rs->cnt = 0;
                rs->step++;
            case 1:
                MercSysMoveScore(wk);
                if (IdIsAnimEnd(&mercId.idsys, 0, ID_MERC_MES)) {
                    FadeSetW(2, 0, 0, 0);
                    MercSysResultInit(wk);
                    disp_bak = pG->flags_58;
                    BitSet(pG->flags_58, 0xFFFFFFFF);
                    BitOff(pG->flags_58, 0x2000);
                    BitOff(pG->flags_58, 0x800);
                    BitOff(pG->flags_58, 0x10000);
                    stop_bak = pG->flags_170;
                    BitSet(pG->flags_170, 0xFFFFFFFF);
                    BitOff(pG->flags_170, 0x00800000);
                    BitOff(pG->flags_170, 0x40);
                    rs->cnt = 0;
                    rs->step++;
                }
                break;
            case 2:
                rs->cnt++;
                if (rs->cnt > 1) {
                    mercId.kill();
                    setLangExt3(data_name + 3);
                    Dvd.FileExistCheck(data_name, &size);
                    size += 0x34;
                    size += MARGIN;
                    swap.SwapOut((u32) pG->pRoomArc, size, 0);
                    pRslt = new MercResult;
                    pRslt->init(wk);
                    wk->strId = SndStrReq(0, 0x3A, 0x80000003, 0, 0, 0.0f);
                    rs->cnt = 0;
                    rs->step++;
                }
                break;
            case 3:
                if (pRslt->move(wk) == 0) {
                    SndStrReq(wk->strId, 8, 0, 0);
                    rs->cnt = 0;
                    rs->step++;
                }
                break;
            case 4:
                if (fadeIsOn(&Fade[2]) == 0) {
                    pRslt->quit();
                    delete pRslt;
                    swap.SwapIn();
                    rs->run = 0;
                }
                break;
            }
            mercId.idsys.move();
            mercId.idsys.trans();
            SceSleep(1);
        } while (rs->run != 0);
        SceSleep(1);
        FadeSetW(2, 0, 0, 0);
        CardSysSave();
        pG->flags_54 |= 0x04000000;
        CamCtrl.Comeback(0);
        SceEventEnd(0);
    }
    return 1;
}

void MercSysGetSaveWork(MercSaveWork* save)
{
    int i;
    int j;

    // pSys read through a reference: its load is not hoisted above the stores through `save`
    // (a plain pSys is a fixed scalar that a varying struct store never aliases). The x10 table
    // goes through a pointer local: a pointer variable carries REG_POINTER, so regclass makes it
    // the base of the `tbl[i]` address and `i*4` the index (GENERAL_REGS: r0, dying at the load);
    // written `SysRef(pSys)->x10[i]` neither operand is flagged, `i*4` becomes BASE_REGS and its
    // longer life drags the rank-pointer giv init to the block end (r8 instead of r12).
    for (i = 0; i < 4; i++) {
        u32* tbl = SysRef(pSys)->x10;
        u32 w = tbl[i];

        save->stage[i].score = (w & 0x0FFFFFFF) * 10;
        save->stage[i].mode = (w >> 28) & 7;
        save->stage[i].newFlag = w >> 31;
        for (j = 0; j < 5; j++) {
            int r = 0;

            if (flagCk(SysRef(pSys)->x20, i * 15 + j * 3)) {
                r = 4;
            }
            if (flagCk(SysRef(pSys)->x20, i * 15 + j * 3 + 1)) {
                r |= 2;
            }
            if (flagCk(SysRef(pSys)->x20, i * 15 + j * 3 + 2)) {
                r |= 1;
            }
            save->rank[j][i] = r;
        }
    }
}

void MercSysSetSaveWork(MercSaveWork* save)
{
    int i;
    int j;

    for (i = 0; i < 4; i++) {
        {
            // Two-set `sc` (the load, then the scaled/masked value): two deaths make it a global
            // pseudo, so local-alloc cannot tie the first `or` to it (the result is tied to the mode
            // operand instead: `or r0,r7,r0`) and global.c gives it r7 after local-alloc took r10/r8
            // for i*12 / pSys.
            u32 sc = save->stage[i].score;
            u32 w;
            sc = (sc / 10) & 0x0FFFFFFF;
            w = (sc | ((save->stage[i].mode & 7) << 28)) | (save->stage[i].newFlag << 31);
            SysRef(pSys)->x10[i] = w;
        }
        for (j = 0; j < 5; j++) {
            int r = save->rank[j][i];

            if (r & 4) {
                flagOn(SysRef(pSys)->x20, i * 15 + j * 3);
            }
            if (r & 2) {
                flagOn(SysRef(pSys)->x20, i * 15 + j * 3 + 1);
            }
            if (r & 1) {
                flagOn(SysRef(pSys)->x20, i * 15 + j * 3 + 2);
            }
        }
    }
}

int MercSysSetPoint(int kind, int pt)
{
    MercSysWork* wk = &MercSysWk;
    u32 idx;
    u32 add;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    if (!(pG->flags_54 & 0x40000000)) {
        return 1;
    }
    if (kind == 9) {
        wk->score += pt;
        return 1;
    }
    if (wk->combo == 1) {
        wk->flags |= MF_COMBO_ON;
    }
    wk->combo++;
    if (wk->maxCombo < wk->combo) {
        wk->maxCombo = wk->combo;
    }
    if (wk->combo > 1) {
        wk->comboTimer = ComboTimerMax;
        wk->flags &= ~MF_COMBO_OFF;
    }
    idx = wk->combo - 1;
    if (idx > 8) {
        idx = 9;
    }
    add = addScoreTbl[kind][idx] - defaultScoreTbl[kind];
    if (wk->bonusTimer > 0) {
        wk->bonusKill++;
        if (add < BonusTimeAdd) {
            add = BonusTimeAdd;
        }
    }
    wk->bonusScore += add;
    wk->score += defaultScoreTbl[kind];
    wk->killCnt++;
    wk->kill++;
    return 1;
}

int MercSysSetAddTime(int sec)
{
    MercSysWork* wk = &MercSysWk;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    if (!(pG->flags_54 & 0x40000000)) {
        return 1;
    }
    wk->addTime += sec;
    wk->flags |= MF_ADD_TIME;
    // no return: the original falls off the end (r3 still holds `sec`)
}

int MercSysSetBonusTime(int frames)
{
    MercSysWork* wk = &MercSysWk;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    if (!(pG->flags_54 & 0x40000000)) {
        return 1;
    }
    if (wk->bonusTimer == 0) {
        wk->bonusKill = 0;
        wk->flags |= MF_BONUS_ON;
    }
    wk->bonusTimer += frames;
    wk->flags &= ~MF_BONUS_OFF;
    return 1;
}

void IdSetTrans(IDSystem* id, int no, u8 type, int on)
{
    IdUnit* u = id->unitPtr(no, type);

    if (u == NULL) {
        pLog->err(0, 0, "IdSetTrans : pIdUnit is NULL");
    } else {
        if (on == 1) {
            u->flags |= 8;
        } else {
            u->flags &= ~8;
        }
    }
}

void IdSetAnmStart(IDSystem* id, int no, u8 type, int on)
{
    IdUnit* u = id->unitPtr(no, type);

    if (u == NULL) {
        pLog->err(0, 0, "IdSetAnmStart : pIdUnit is NULL");
    } else {
        if (on == 1) {
            u->dir &= ~0xF;
        } else {
            u->dir |= 0xF;
        }
        id->setTime(u, 0);
    }
}

void IdSetColInit(IDSystem* id, int no, u8 type)
{
    IdUnit* u = id->unitPtr(no, type);

    if (u == NULL) {
        pLog->err(0, 0, "IdSetColInit : pIdUnit is NULL");
    } else {
        u->col[0] = 255.0f;
        u->col[1] = 255.0f;
        u->col[2] = 255.0f;
        u->col[3] = 255.0f;
        u->curve[2] = NULL;
    }
}

static void IdSetColLoop(IDSystem* id, int no, u8 type, int on)
{
    IdUnit* u = id->unitPtr(no, type);

    if (u == NULL) {
        pLog->err(0, 0, "IdSetColInit : pIdUnit is NULL");
    } else {
        if (on == 1) {
            u->loop |= 4;
        } else {
            u->loop &= ~4;
        }
    }
}

void IdSetColStart(IDSystem* id, int no, int src, u8 type)
{
    IdUnit* u = id->unitPtr(no, type);
    IdUnit* s = id->unitPtr(src, type);

    if (u == NULL || s == NULL) {
        pLog->err(0, 0, "IdSetColStart : pIdUnit is NULL");
    } else {
        u->col0[0] = s->col0[0];
        u->col0[1] = s->col0[1];
        u->col0[2] = s->col0[2];
        u->col0[3] = s->col0[3];
        u->col1[0] = s->col1[0];
        u->col1[1] = s->col1[1];
        u->col1[2] = s->col1[2];
        u->col1[3] = s->col1[3];
        u->curve[2] = s->curve[2];
        u->timer[2] = 0;
    }
}

// Shows `val` (clamped to `max`) as `digits` decimal digits on the units no..no+digits-1;
// mode 0 hides leading zeros.
void IdSetNum(IDSystem* id, int no, u8 type, int val, int max, int digits, int mode)
{
    int d[32];
    int show;
    int i;

    if (val > max) {
        val = max;
    }
    for (int j = 0; j < digits; j++) {
        d[j] = val % 10;
        val /= 10;
    }
    show = mode;
    for (i = digits - 1; i >= 0; i--) {
        IdUnit* u = id->unitPtr(no + i, type);

        if (u == NULL) {
            pLog->err(0, 0, "IdSetNum : pIdUnit is NULL");
            return;
        }
        if (show == 0 && d[i] == 0 && i != 0) {
            u->flags &= ~8;
        } else {
            u->flags |= 8;
            show = 1;
            u->flags_7F |= 2;
            u->no = d[i];
        }
    }
}

void IdSetTexNo(IDSystem* id, int no, u8 type, int texNo)
{
    IdUnit* u = id->unitPtr(no, type);

    if (u == NULL) {
        pLog->err(0, 0, "IdSetTexNo : pIdUnit is NULL");
    } else {
        u->no = texNo;
        u->flags_7F |= 2;
    }
}

int IdIsAnimEnd(IDSystem* id, int no, u8 type)
{
    IdUnit* u = id->unitPtr(no, type);

    if (u != NULL) {
        return (u->end & 3) ? 1 : 0;
    }
    pLog->err(0, 0, "IdIsAnimEnd : pIdUnit is NULL");
    return 1;
}

void MercID::init(int num)
{
    static char data_name[] = "SS/___/id400.dat";
    void* addr;

    setLangExt3(data_name + 3);
#line 1715 "D:/Bio4/Prog/mercenaries.cpp"
    Dvd.ReadCheck(DVD_READ_N(data_name, 0, 0, 0, 0, 5), 0, 0, &addr);
    pData = addr;
    idsys.gameInit(num);
    idsys.roomInit();
    pTex = DATA_PTR(pData, 0x10);
    pIdMain = DATA_PTR(pData, 0x14);
    pIdStart = DATA_PTR(pData, 0x18);
    pIdTimeUp = DATA_PTR(pData, 0x1C);
    set();
    idsys.set(pIdMain, 0xFF, ID_MERC, 0x13, 5, 0);
    IdSetTrans(&idsys, 0x20, ID_MERC, 0);
    IdSetTrans(&idsys, 0x60, ID_MERC, 0);
    IdSetTrans(&idsys, 0, ID_MERC, 0);
    IdSetTrans(&idsys, 0x10, ID_MERC, 0);
    IdSetTrans(&idsys, 0x30, ID_MERC, 0);
    IdSetTrans(&idsys, 0x40, ID_MERC, 0);
}

void MercID::set()
{
    IdTexRelease(6);
    IdTexDataLoad(pTex, 6);
}

void MercID::kill()
{
    IdTexRelease(6);
    idsys.kill(0xFF, ID_MERC);
    idsys.kill(0xFF, ID_MERC_MES);
}

void MercID::dispMissionStart()
{
    idsys.set(pIdStart, 0xFF, ID_MERC_MES, 0x13, 4, 0);
    SndCall(6, 0x7C, 0, 0, 0, 0);
}

void MercID::dispTimeUp()
{
    idsys.set(pIdTimeUp, 0xFF, ID_MERC_MES, 0x13, 4, 0);
    SndCall(6, 0x7E, 0, 0, 0, 0);
}

int MercResult::init(MercSysWork* wk)
{
    static char data_name[] = "SS/___/omk_r1.dat";
    void* addr;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    setLangExt3(data_name + 3);
#line 1866 "D:/Bio4/Prog/mercenaries.cpp"
    Dvd.ReadCheck(DVD_READ_N(data_name, 0, 0, 0, 0, 5), 0, 0, &addr);
    pData = addr;
    IdTexRelease(4);
    IdSys.roomInit();
    pTex = DATA_PTR(pData, 0x10);
    pIdRank[0] = DATA_PTR(pData, 0x14);
    pIdRank[1] = DATA_PTR(pData, 0x18);
    pIdRank[2] = DATA_PTR(pData, 0x1C);
    pIdRank[3] = DATA_PTR(pData, 0x20);
    pIdRank[4] = DATA_PTR(pData, 0x24);
    pIdExtra = DATA_PTR(pData, 0x28);
    pIdEnd = DATA_PTR(pData, 0x2C);
    IdTexDataLoad(pTex, 7);
    IdSys.set(pIdRank[wk->rslt.mode], 0xFF, ID_RESULT, 0x13, 6, 0);
    step = 0;
    cnt = 0;
    x32 = 0;
    x33 = 0;
    return 1;
}

int MercResult::move(MercSysWork* wk)
{
    int mes[4];

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    mes[0] = wk->mes[5];
    mes[1] = wk->mes[6];
    mes[2] = wk->mes[7];
    mes[3] = wk->mes[8];
    switch (step) {
    case 0:
        FadeSetW(0x80000002, 10, 0, 0);
        step++;
        break;
    case 1:
        IdSetNum(&IdSys, 0x11, ID_RESULT, wk->rslt.kill, 9999, 4, 0);
        IdSetNum(&IdSys, 0x21, ID_RESULT, wk->rslt.score, 999999, 6, 0);
        IdSetNum(&IdSys, 0x31, ID_RESULT, wk->rslt.maxCombo, 999, 3, 0);
        for (int i = 1; i <= 5; i++) {
            IdSetTrans(&IdSys, i, ID_RESULT, i <= wk->rslt.rank);
        }
        IdSetTrans(&IdSys, 0, ID_RESULT, 1);
        IdSetTexNo(&IdSys, 0, ID_RESULT, wk->rslt.hiMode);
        IdSetNum(&IdSys, 0x41, ID_RESULT, wk->rslt.hiScore, 999999, 6, 0);
        if (Key.trg & KEY_A) {
            FadeSetW(2, 10, 0, 0);
            if (flagCk(&wk->flags, mercSysGetFlag[wk->stage])) {
                step = 0xA;
            } else if (wk->flags & MF_ALL_RANK) {
                step = 0x14;
            } else {
                return 0;
            }
        }
        break;
    case 0xA:
        if (Fade[2].flags & 1) {
            break;
        }
        FadeSetW(0x80000002, 10, 0, 0);
        IdSys.kill(0xFF, ID_RESULT);
        IdSys.set(pIdExtra, 0xFF, ID_RESULT, 0x13, 4, 0);
        for (int i = 0; i < 4; i++) {
            int on = 0;

            if (flagCk(SYS_FLAG_TBL, extFlagTbl[i])) {
                on = 1;
            }
            IdSetTrans(&IdSys, i + 1, ID_RESULT, on);
        }
        IdSetTrans(&IdSys, wk->stage + 1, ID_RESULT, 1);
        IdSetAnmStart(&IdSys, wk->stage + 1, ID_RESULT, 1);
        IdSetColStart(&IdSys, wk->stage + 1, 0, ID_RESULT);
        cnt = 0;
        step++;
        break;
    case 0xB:
        if (Fade[2].flags & 1) {
            break;
        }
        cnt++;
        if (cnt > 29) {
            SceMesSet(mes[wk->stage], 0xF0, 1, 100, MES_Y(cMes.getWork()));
            step++;
        }
        break;
    case 0xC:
        if (Key.trg & KEY_A) {
            MessageControl* m = &cMes;

            for (int i = 0; i < 16; i++) {
                m->Delete(i);
            }
            FadeSetW(2, 10, 0, 0);
            if (wk->flags & MF_ALL_RANK) {
                step = 0x14;
            } else {
                return 0;
            }
        }
        break;
    case 0x14:
        if (Fade[2].flags & 1) {
            break;
        }
        FadeSetW(0x80000002, 10, 0, 0);
        IdSys.kill(0xFF, ID_RESULT);
        IdSys.set(pIdEnd, 0xFF, ID_RESULT, 0x13, 4, 0);
        cnt = 0;
        step++;
        break;
    case 0x15:
        if (Fade[2].flags & 1) {
            break;
        }
        cnt++;
        if (cnt > 29) {
            SceMesSet(wk->mes[9], 0xF0, 1, 100, MES_Y(cMes.getWork()));
            step++;
        }
        break;
    case 0x16:
        if (Key.trg & KEY_A) {
            MessageControl* m = &cMes;

            for (int i = 0; i < 16; i++) {
                m->Delete(i);
            }
            FadeSetW(2, 10, 0, 0);
            return 0;
        }
        break;
    }
    return 1;
}

void MercResult::quit()
{
    Cckpt.roomInit();
    Cckpt.move();
}

void AdaResult::init(int no)
{
    static char data_name[] = "SS/___/omk_r0.dat";
    void* addr;

    setLangExt3(data_name + 3);
#line 2141 "D:/Bio4/Prog/mercenaries.cpp"
    Dvd.ReadCheck(DVD_READ_N(data_name, 0, 0, 0, 0, 5), 0, 0, &addr);
    pData = addr;
    IdTexRelease(4);
    IdSys.roomInit();
    pTex = DATA_PTR(pData, 0x10);
    pId = DATA_PTR(pData, 0x14);
    IdTexDataLoad(pTex, 7);
    IdSys.set(pId, 0xFF, ID_RESULT, 0x13, 6, 0);
    step = 0;
    cnt = 0;
    x32 = 0;
    x33 = 0;
}

int AdaResult::move(int mesNo)
{
    int i;

    switch (step) {
    case 0:
        FadeSetW(0x80000002, 10, 0, 0);
        IdSys.set(pId, 0xFF, ID_RESULT, 0x13, 4, 0);
        cnt = 0;
        step++;
        break;
    case 1:
        if (Fade[2].flags & 1) {
            break;
        }
        cnt++;
        if (cnt > 29) {
            SceMesSet(mesNo, 0xF0, 1, 100, MES_Y(cMes.getWork()));
            step++;
        }
        break;
    case 2:
        if (Key.trg & KEY_A) {
            MessageControl* m = &cMes;

            for (i = 0; i < 16; i++) {
                m->Delete(i);
            }
            FadeSetW(2, 10, 0, 0);
            return 0;
        }
        break;
    }
    return 1;
}

void AdaResult::quit()
{
    Cckpt.roomInit();
    Cckpt.move();
}

int CountDown::checkState(u32 bit)
{
    return (flags & bit) ? 1 : 0;
}
