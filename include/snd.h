#ifndef SND_H
#define SND_H

// Game-side sound interface (D:/Bio4/Prog/snd.cpp, game/snd, -O2, C++ linkage). Wraps the C sound
// driver in include/snd_drv.h. Field offsets come from the disassembly; only extend.

#include "types.h"
#include "vec.h"
#include "cManager.h"

// Bit `no` of a u32 bitmap, MSB first (block loaded flags, callErr).
#define SND_BIT_CK(a, no) (*((a) + ((u32) (no) >> 5)) & (0x80000000 >> ((no) & 31)))
#define SND_BIT_SET(a, no) { u32* p_ = (a); p_[(u32) (no) >> 5] |= (0x80000000 >> ((no) & 31)); }
#define SND_BIT_CLR(a, no) { u32* p_ = (a); p_[(u32) (no) >> 5] &= ~(0x80000000 >> ((no) & 31)); }

// Reverb parameters (room header `STB` efx[0] = DPL2, efx[1] = stereo).
struct SndEfxParam {
    u16 aux_core;    // 0x00  default aux A per block type (low bytes)
    u16 aux_em;      // 0x02
    u16 aux_wep;     // 0x04
    u16 aux_room;    // 0x06
    f32 preDelay;    // 0x08
    f32 time;        // 0x0C
    f32 coloration;  // 0x10
    f32 damping;     // 0x14
    f32 mix;         // 0x18
    f32 crosstalk;   // 0x1C
};

// Room sound header (`STB` sub-file of the room archive, pSnd->hdr; DefEffTbl when missing).
struct SndRoomHdr {
    SndEfxParam efx[2];   // 0x00
    u32 curve_sel[32];    // 0x40   offsets to SndCurveSel, indexed by SND_SIT::curve_no
    u32 vol_ofs[32];      // 0xC0   offsets to SndCurveTbl (volume by distance)
    u32 pitch_ofs[32];    // 0x140  offsets to SndCurveTbl (pitch by distance)
    u32 filter_ofs[32];   // 0x1C0  offsets to SndCurveTbl (filter by distance)
};

// Which distance curves a SIT uses (SndRoomHdr::curve_sel target).
struct SndCurveSel {
    s8 svol;         // 0x00
    s8 vol;          // 0x01
    s8 pitch[2];     // 0x02  [DPL2, stereo]
    s8 filter[2];    // 0x04
};

struct SndCurveEnt {
    f32 dist;        // 0x00
    u16 x4;
    u16 val;         // 0x06  read as s16 (pitch), s8 at 0x07 (filter), u8 at 0x07 (volume)
};

struct SndCurveTbl {
    u32 num;         // 0x00
    f32 scale;       // 0x04  applied to every entry's dist at room start
    SndCurveEnt e[1];// 0x08
};

// Stream block file (SndMem.str_file[]).
struct SndStrEnt {
    u32 x0;
    u8 vol;          // 0x04
    u8 pad_5[11];
};
struct SndStrFile {
    u32 num;         // 0x00
    u32 x4;
    u32 ent_ofs;     // 0x08  offset to SndStrEnt[num]
};

// Door SE file (SndMem.door_tbl): offsets to a u32 file table and a u16 count.
struct SndDoorTbl {
    u32 file_ofs;    // 0x00
    u32 num_ofs;     // 0x04
};

// Room BGM/stream table file (SndMem.bgm_tbl).
struct SndBgmEnt {
    u32 id;          // 0x00
    u32 bgm[6];      // 0x04
    u32 str[6];      // 0x1C
};
struct SndBgmRoom {
    u32 num;         // 0x00
    SndBgmEnt e[1];  // 0x04
};
struct SndBgmTbl {
    u32 room_ofs;    // 0x00  offset to u32 offsets (one per room, relative to that array)
    u32 list_ofs;    // 0x04  offset to the u16 room id list, 0xFFFF terminated
};

// Room save record bytes used here (cRoomData::getRoomSavePtr).
struct SndRoomSave {
    u8 pad_0[0xA8];
    u32 bgm[6];      // 0xA8  room BGM table: slot 0 low half, slot 1 high half
    u32 str[6];      // 0xC0  room stream table
};

struct SndMute {
    s32 on;          // 0x00
    u8 vol;          // 0x04  master volume saved while muted
    u8 pad_5[3];
};

// BGM sequence / stream slot (Snd.bgm_work[2], Snd.str_work[4]).
struct SndPlayWork {
    u32 used : 8;    // 0x00
    u32 stat : 8;    // 0x01  1 = stopped / faded out by the game
    s32 vol : 8;     // 0x02
    s32 vol_def : 8; // 0x03
    u32 id;          // 0x04
    s16 no;          // 0x08
    u16 blk;         // 0x0A
    u8 mute_vol;     // 0x0C  BGM volume saved by SndRoomBgmMute
    u8 pad_D;
    u16 timer;       // 0x0E  frames a stopped stream has been waiting
};

// Positional SE being tracked by sndSurroundCalc (Snd.sur[48]).
struct SndSurWork {
    u8 type;         // 0x00  0x80 | seq flag
    s8 svol_ofs;     // 0x01
    s8 vol_ofs;      // 0x02
    s8 pitch_ofs;    // 0x03
    s8 filter_ofs;   // 0x04
    u8 pad_5[3];
    s32 inner;       // 0x08
    s32 vol_calc;    // 0x0C
    s32 pan_calc;    // 0x10
    u16 blk;         // 0x14
    u16 no;          // 0x16
    u32 id;          // 0x18
    Vec pos;         // 0x1C
    Vec* ppos;       // 0x28  live position (followed while obj is alive)
    cUnit* obj;      // 0x2C
};

struct SndEmHist {
    u16 used;        // 0x00
    u16 id;          // 0x02
    u16 timer;       // 0x04
    u16 no;          // 0x06
};

// Game sound work (`Snd`, 0xAE8 bytes, pSnd).
struct SndWork {
    SndMute mute[4];         // 0x00  core/pl, em, ... (SndMuteSet bits 0x10..0x80)
    u32 blk_flag[1];         // 0x20  block loaded bits (SND_BIT_*)
    SndPlayWork bgm_work[2]; // 0x24
    SndPlayWork str_work[4]; // 0x44
    u8* bgm_mram;            // 0x84  BGM MRAM allocation top (dvd.cpp grows it down)
    u32 bgm_aram;            // 0x88  BGM ARAM allocation top (grows down)
    u8 bgm_id[2];            // 0x8C
    u16 door_no;             // 0x8E  door SE table loaded
    s32 room_ok;             // 0x90  room sound data initialised
    struct SeAtHead* se_at;  // 0x94  room "ESE" sound area data (se_at.cpp), NULL when none
    struct SeAt* se_at_list; // 0x98  its records
    SndRoomHdr* hdr;         // 0x9C
    SndSurWork sur[48];      // 0xA0
    SndEmHist em_hist[32];   // 0x9A0
    u8* mram_top;            // 0xAA0  MRAM allocation pointer (dvd.cpp)
    u32 aram_top;            // 0xAA4  ARAM allocation pointer (dvd.cpp)
    u8 em_id[8];             // 0xAA8  enemy id per enemy block (6 used)
    u32 room_bgm[6];         // 0xAB0  [0] current, [1..5] by pG->snd_tbl_no
    u32 room_str[6];         // 0xAC8
    u8 str_no[2];            // 0xAE0
    s16 bgm_at[2];           // 0xAE2  floor attribute BGM control applied per slot
    u8 pad_AE6[2];
};

// "ESE" room file header (game/se_at.cpp), followed by the SeAt records at 0x10.
struct SeAtHead {
    char magic[4];   // 0x00  "ESE"
    u16 version;     // 0x04  0x100
    u16 num;         // 0x06  record count
    u8 pad_8[8];
};

// Timed / area sound entry (game/se_at.cpp), 0x2C bytes.
struct SeAt {
    u8 flags;        // 0x00  bit0: enabled (SeAtSetOnOff)
    u8 no;           // 0x01  id (GetSeAtPtr)
    u16 flags2;      // 0x02  bit0: no position
    Vec pos;         // 0x04
    u16 x10;         // 0x10
    u16 blk;         // 0x12  SndCall block
    u16 x14;         // 0x14
    u16 se_no;       // 0x16  SndCall number
    u16 interval;    // 0x18  fixed interval, 0 = random (rnd_base + Rnd() % rnd_range)
    u16 wait;        // 0x1A  first-play delay
    u16 cnt;         // 0x1C  frames until the next play
    s16 repeat;      // 0x1E  plays left (0 = endless), -1 = finished
    u16 rnd_base;    // 0x20
    u16 rnd_range;   // 0x22
    u8 pad_24[0x2C - 0x24];
};

// game/se_at.cpp
void SeAtCheck();
extern "C" {
void SeAtInit();
int SeAtSetOnOff(int no, int on);
SeAt* GetSeAtPtr(int no);
u32 SeAtSndCall(int no);
}

// ARAM / MRAM sound data map (`SndMem`, 0xA0 bytes).
struct SndMemWork {
    SndStrFile* str_file[2]; // 0x00
    u32* bgm_file;           // 0x08  BGM file numbers
    SndDoorTbl* door_tbl;    // 0x0C
    SndBgmTbl* bgm_tbl;      // 0x10
    u8* blk_mram[14];        // 0x14  per block MRAM data address (dvd.cpp fills it)
    u32 blk_aram[14];        // 0x4C  per block ARAM sample address
    u8* mram_end;            // 0x84  end of the fixed sound data in MRAM
    u32 str_buf[4];          // 0x88  stream buffers
    u32 sub_adr;             // 0x98  sub screen sound data
    u32 sub_end;             // 0x9C
};

// Recent SndCall log (debug display, 25 entries).
struct SndHistory {
    s8 idx;          // 0x00
    s8 num;          // 0x01
    s8 top;          // 0x02
    u8 blk[25];      // 0x03
    u16 no[25];      // 0x1C
    s8 vol[25];      // 0x4E
    s8 svol[25];     // 0x67
    s8 pan[25];      // 0x80
    s8 span[25];     // 0x99
};

extern SndWork Snd;
extern SndMemWork SndMem;
extern u32 UseAramSize[14];
extern SndHistory History;
extern SndRoomHdr DefEffTbl;
// no `extern u32 aram_buf[3]` here: uninitialised objects (static or not) are emitted in
// first-declaration order, and snd.cpp's `static callErr` precedes aram_buf in the original .bss
extern u16 StrFileTbl[2];
extern int str_flag;
extern u32 ARAM_FREE_BASE;
// pSnd is loaded as a struct member (the pLog trick, db_log.h): a store through it makes GCC 2.95
// reload the pointer before the next use and keep the stores in source order, which is what most of
// snd.cpp shows. `pSndRaw` is the same symbol seen as a plain pointer (scalar load, no reloads) for
// the few functions whose code only matches that way.
struct SndWorkPtr {
    SndWork* p;
    SndWork* operator->() { return p; }
};
extern SndWorkPtr pSnd;
extern SndWork* pSndRaw __asm__("pSnd");
extern u32 SndStrAramAddr[4];

void SndInit();
void SndInit2();
void SndDriverInit();
void SndSystemReset();

// SndCall(blk, no, pos, id, vol, obj): blk 0 core, 1 player, 2 weapon, 3/4 BGM, 5 foot, 6 room,
// 7 door, 8.. enemies (id selects the enemy block). vol: 0 = from the SIT, 0x100/0x200/0x400 set
// Snd_ctrl_work.x56 bits, 0x80000000 follow pos. Returns the sound id (0 = not played).
u32 SndCall(u16 blk, u16 no, Vec* pos, int id, int vol, cUnit* obj);
u32 EmSeCall(int no, int id, Vec* pos, int vol0, int vol1, cUnit* obj);
u32 RoomSeCall(int no, Vec* pos, int vol0, int vol1, cUnit* obj);
u32 PlSeCall(int no, Vec* pos, int vol0, int vol1, cUnit* obj);
u32 CoreSeCall(int no, Vec* pos, int vol0, int vol1, cUnit* obj);
u32 FootSeCall(int no, Vec* pos, int vol0, int vol1);
u32 DoorSeCall(int no);
int SndSetVol(u32 id, int vol, int time);
int SndSetDopPitch(u32 id, int pitch);
int SndStop(u32 id, int time);
void SndBlkStop(int blk);
int SndEndCheck(u32 id);

u32 SndStrReq(int blk, int no, int req, int time, int vol, f32 pos);
int SndStrReq(u32 id, int req, int time, int vol);
int SndStrStatusCk(int blk, int no, u32 status);
int SndStrStatusCk(u32 id, u32 status);
int SndStrVolSet(int blk, int no, int time, int vol);
int SndStrVolReset(int blk, int no, int time);

void SndWatcher();
void SndNextRoomInit();
void SndReadAddrInit();
int SndRoomStartInit();
int SndDoorSeLoad();
void SndRoomBgmLoad();
void SndRoomBgmStartCheck(int reset);
int SndRoomBgmStart(u8 no, int vol);
void SndRoomBgmStop(u8 no, int time);
int SndRoomBgmVolSet(u8 no, int vol, int time);
int SndRoomBgmVolReset(u8 no, int time);
int SndRoomBgmMute(u8 no, int on, int time);
void SndRoomBgmMuteAll(int on, int time);
void SndRoomStrStartCheck();
void SndRoomStrStart(int a, int time, int loop);
void SndRoomStrStop(int time);
int SndRoomStrVolSet(int vol, int time);
int SndRoomStrVolReset(int time);

void SndMuteSet(int bits, int on);
int SndSetMasterVol(u32 type, int vol);
int SndGetMasterVol(u32 type);
void SndSetOutputMode(int mode, int init);
int SndStopCheck();
void SndAllStop();
void SndAllFadeOut();
void SndSePause(int on, s16 type);
void SndSeAbsPause();
void SndSePauseAll(int on);
void SndSoftReset();
void SndBgmTblInit();   // rebuild the room BGM table (game: cGameSave::load after clearGlobalSaveData)
int SndBgmTblSet(u16 room, int no);
void SndBgmTblSetEnable(int type, int save);
void SndBgmTblSetDisable(int type, int save);
void SndSubScreenInit();
void SndSubScreenExit();
void SndEventStrStop(int time);
void SndEventInit();
void SndEventEnd();
int SndEmDataReadCheck(int id);
void SndBlkInit(int type, int id, int no);
void SndBgmLoad(int no);
int SndBgmDataReadCheck(int id);
void SndSetReverb();
int SndStatDisp(int req);
void SndSeAbsFadeOutAll_sec(int sec);
void SndSeAbsFadeOutAll_5msec(s16 time);
void SndSeqFadeOutAll_sec(u8 type, int sec);

#endif
