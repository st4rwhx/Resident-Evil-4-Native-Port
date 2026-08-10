#ifndef MERCENARIES_H
#define MERCENARIES_H

#include "types.h"
#include "vec.h"
#include "id_sys.h"

class cObj;

// Result block at the head of the Mercenaries work (MercSysResultInit fills it).
struct MercResultInfo {
    u32 score;       // 0x00  final score
    u32 time;        // 0x04  remaining time in 1/100 s
    int maxCombo;    // 0x08
    int kill;        // 0x0C
    int mode;        // 0x10  character (MercSysWork::mode)
    int rank;        // 0x14  RankTbl index reached (0..5)
    u32 hiScore;     // 0x18  saved best score of the stage
    int hiMode;      // 0x1C  character that made it
    int newRecord;   // 0x20  1 = this run is the new best
};

// MercSysResultMove task state (MercSysWork+0xC0).
struct MercRsltSt {
    u8 run;          // 0x00
    u8 step;         // 0x01
    u8 cnt;          // 0x02
    u8 pad_3;
    u32 x4;          // 0x04
    u32 x8;          // 0x08
};

// Mercenaries minigame work (game/mercenaries.cpp `MercSysWk`, 0xCC bytes).
struct MercSysWork {
    MercResultInfo rslt;  // 0x00
    u32 score;       // 0x24
    int combo;       // 0x28
    int comboTimer;  // 0x2C  frames left of the combo
    int x30;         // 0x30
    int killCnt;     // 0x34  kills registered this frame (MercSysMoveScore bonus)
    int kill;        // 0x38
    int bonusKill;   // 0x3C  kills during the bonus time
    int maxCombo;    // 0x40
    int bonusScore;  // 0x44  pending bonus points (shown, then added to score)
    int bonusDisp;   // 0x48  bonus points being shown
    int addTime;     // 0x4C  seconds to add (MercSysSetAddTime)
    int bonusTimer;  // 0x50  bonus time frames left
    u32 sndId;       // 0x54  time warning SE handle
    int mode;        // 0x58  character: 0 Leon, 1 Ada, 2 Krauser, 3 HUNK, 4 Wesker (pG->x4FB8 0/2/4/3/5)
    int stage;       // 0x5C  0..3 from the room id (400, 402, 403, 404)
    u32 flags;       // 0x60
    cObj* smd;       // 0x64  dummy model (SetObjSmd)
    u32 x68;
    u32 strId;       // 0x6C  SndStrReq handle
    void* x70;       // 0x70  copies of the MercInit block
    void* smdMot;    // 0x74  MotionSetCore data of the dummy model
    u32 x78;         // 0x78
    int mesStart;    // 0x7C  start message
    int mes[10];     // 0x80  [4]: x4FB8 == 4 message, [5..8]: rank messages, [9]: result end message
    int mesA8;       // 0xA8
    int mesAC;       // 0xAC
    u32 xB0;         // 0xB0
    u8 startSt[5];   // 0xB4  MercSysMoveStart: [0] running, [1] step
    u8 mainSt[5];    // 0xB9  MercSysMoveMain: [0] running
    u8 pad_BE[2];
    MercRsltSt rsltSt;  // 0xC0  MercSysResultMove state (sizeof == 0xCC)
};

// Room start parameters handed to MercSysInitRoom.
struct MercInit {
    Vec pos;         // 0x00  player start position
    Vec rot;         // 0x0C
    void* x18;       // 0x18
    void* smdMot;    // 0x1C
    u32 x20;         // 0x20
    int mesStart;    // 0x24
    int mes[10];     // 0x28
    int mesA8;       // 0x50
    int mesAC;       // 0x54
    u32 x58;         // 0x58
};

// Save data view of the Mercenaries records (pSys->x10[] / pSys->x20[] bits), 0x80 bytes.
struct MercSaveWork {
    struct {
        u32 score;   // 0x00
        u32 mode;    // 0x04
        u32 newFlag; // 0x08
    } stage[4];      // 0x00
    int rank[5][4];  // 0x30  [mode][stage]
};

extern MercSysWork MercSysWk;

// Mercenaries HUD id data (game/mercenaries.cpp `mercId`, 0x64 bytes).
class MercID {
public:
    void* pTex;        // 0x00  id400.dat sub-files
    void* pIdMain;     // 0x04  type 0x22
    void* pIdStart;    // 0x08  type 0x2C "mission start"
    void* pIdTimeUp;   // 0x0C  type 0x2C "time up"
    void* pData;       // 0x10
    IDSystem idsys;    // 0x14

    void init(int num);
    void set();
    void kill();
    void dispMissionStart();
    void dispTimeUp();
};

extern MercID mercId;

// Mercenaries result screen (omk_r1.dat), new'd by MercSysResultMove (0x34 bytes).
class MercResult {
public:
    void* pTex;        // 0x00
    void* pIdRank[5];  // 0x04  result id data per character
    void* pIdExtra;    // 0x18  unlock screen
    void* pIdEnd;      // 0x1C
    u8 pad_20[0xC];
    void* pData;       // 0x2C
    s8 step;           // 0x30
    s8 cnt;            // 0x31
    u8 x32;
    u8 x33;

    int init(MercSysWork* wk);
    int move(MercSysWork* wk);
    void quit();
};

// Assignment Ada result screen (omk_r0.dat).
class AdaResult {
public:
    void* pTex;        // 0x00
    void* pId;         // 0x04
    u8 pad_8[0x24];
    void* pData;       // 0x2C
    s8 step;           // 0x30
    s8 cnt;            // 0x31
    u8 x32;
    u8 x33;

    void init(int no);
    int move(int mesNo);
    void quit();
};

extern int MercSec;
extern int MercCes;
extern int ComboTimerMax;
extern int ComboTimerFlash;
extern u32 mercSysGetFlag[4];
extern u32 extFlagTbl[4];
extern u32 RankTbl[4][6];

extern "C" {
int MercSysInitStage();
int MercSysInitRoom(MercInit* pMInit);
int MercSysMoveStart(MercSysWork* wk);
int MercSysMoveScore(MercSysWork* wk);
int MercSysMoveMain(MercSysWork* wk);
int MercSysResultInit(MercSysWork* wk);
int MercSysResultMove(MercSysWork* wk);
void MercSysGetSaveWork(MercSaveWork* save);
void MercSysSetSaveWork(MercSaveWork* save);
int MercSysSetPoint(int kind, int pt);
int MercSysSetAddTime(int sec);
int MercSysSetBonusTime(int frames);
// id unit helpers (IDSystem `id`, unit `no` of table `type`)
void IdSetTrans(IDSystem* id, int no, u8 type, int on);
void IdSetAnmStart(IDSystem* id, int no, u8 type, int on);
void IdSetColInit(IDSystem* id, int no, u8 type);
void IdSetColStart(IDSystem* id, int no, int src, u8 type);
void IdSetNum(IDSystem* id, int no, u8 type, int val, int max, int digits, int mode);
void IdSetTexNo(IDSystem* id, int no, u8 type, int texNo);
int IdIsAnimEnd(IDSystem* id, int no, u8 type);
}

#endif
