#ifndef GAME_H
#define GAME_H

#include "types.h"

// game/game.cpp: save data front end.

// Copy of the GlobalWork tail (0x4F80 .. 0x8678) kept in the save data (cGameSave::save / load).
struct GameSaveBlock {
    u8 pad_0[4];              // 0x4F80
    s32 point;                // 0x4F84  pG->point (GamePointBossReset refreshes it)
    u8 pad_8[0x36F8 - 0x8];
};

// Save data image (cGameSave::alloc). The section pointers are stored as offsets from the image
// start while it travels (calcOffset) and turned back into addresses by calcAddr; `base` is 0 in
// the offset form.
struct GameSaveData {
    GameSaveData* base;       // 0x00  the image's own address (0 = offsets)
    u32 size;                 // 0x04
    GameSaveBlock* pGlobal;   // 0x08  offset 0x40
    void* pItem;              // 0x0C  cItemMgr::save/load
    void* pRoom;              // 0x10  cRoomData::save/load (0x3740)
    u32* pSscrn;              // 0x14  SscrnDataSave/Load
    void* pMerchant;          // 0x18  MerchantDataSave/Load
};

class cGameSave {
public:
    u8 pad_0;

    GameSaveData* alloc();
    int load(void* data);
    // The original body also reads a `mode` from r5 although the mangled name says one parameter
    // (sce_com calls it this way); new callers use the GameSaveSave alias below (int result).
    void save(void* data);
    void checkAddr(GameSaveData* data);
    void calcOffset(GameSaveData* data, void* base);
    void calcAddr(GameSaveData* data);
};

int GameSaveSave(cGameSave* g, void* data, int mode) asm("save__9cGameSavePv");

extern GameSaveData* pSaveData;  // .sbss order: pSaveData before GameSave
extern cGameSave GameSave;

// Died demo task parameter (DiedemoExec -> gameDiedemo).
struct DiedemoWork {
    int time;   // 0x00  frames before the demo starts
    int type;   // 0x04  0 normal, 1 with the sub character alive, 2 (flags_54 bit31)
};

extern "C" {
void GameTask();
void primInit();
void primFree();
void GameLoad();
void GameContinue(int mode);
void GamePointInit(u32 mode);
void GameAddPoint(int type);
void GamePointBossReset();
void PrimDispWorkNum(int x, int y, int col);
void DiedemoExec(int time, int type);
void gameDiedemoCheck();
void gameDiedemo(DiedemoWork* w);
}
void GameStopModeEnd();

#endif
