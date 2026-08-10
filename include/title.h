#ifndef TITLE_H
#define TITLE_H

#include "types.h"

struct IdUnit;

// Title screen task work (game/title.cpp, mem_calloc'd 0x9C bytes by Title_task).
struct TitleWork {
    s8 mode;          // 0x00  titleFuncTbl index (0 init, 1 wait, 2 nintendo, 3 warning, 4 logo, 5 main, 6 sub/omake, 7 exit)
    s8 step;          // 0x01  state inside the mode
    s8 sub;           // 0x02  sub state (demo movie steps, stage select)
    u8 x3;            // 0x03
    u8 demoNo;        // 0x04  alternates between the two demo movies
    u8 pad_5[3];
    int sndFlag;      // 0x08  1 = title.snd read done, BGM not started yet (titleLogo)
    u8 xC;            // 0x0C
    u8 pad_D[7];
    int req;          // 0x14  DvdReadN request
    u32 sndId;        // 0x18  SndCall handle of the title BGM
    struct TitleArc* pDat;  // 0x1C  title.dat (offset table)
    int cnt;          // 0x20  frame counter (setTime reads its low half)
    int menuNum;      // 0x24  menu entries
    int cursor;       // 0x28
    IdUnit* menu[5];  // 0x2C  menu id units
    int scroll;       // 0x40  1 = the background scroll follows the stick (titleLoop)
    f32 speed;        // 0x44  background scroll speed
    int x48;          // 0x48  1 = the title logo time was pushed forward (debug menu)
    struct TitleArc* pOmk;  // 0x4C  omk_tX.dat (offset table)
    int omkSize;      // 0x50
    s8 omkCursor;     // 0x54  omake menu: 0 start, 1 back
    s8 omkChar;       // 0x55  mercenaries character select (0..4)
    s8 omkStage;      // 0x56  mercenaries stage select (0..3)
    u8 saveStep;      // 0x57  mode 5 state saved while the omake screens run
    u8 saveSub;       // 0x58
    u8 saveX3;        // 0x59
    u8 pad_5A[2];
    int saveCnt;      // 0x5C
    s8 dbgStage;      // 0x60  debug menu: stage
    s8 dbgRoom[10];   // 0x61  debug menu: room index per stage
    s8 dbgPoint;      // 0x6B  debug menu: jump point
    s8 dbgCursor;     // 0x6C  debug menu: line (0..20)
    s8 dbgEmList;     // 0x6D  debug menu: enemy list
    u8 loadNo;        // 0x6E  debug menu: load slot (1..10)
    u8 pad_6F;
    s16 dbgX;         // 0x70  debug menu position
    s16 dbgY;         // 0x72
    u8 pad_74[0x9C - 0x74];
};

// Offset table at the head of title.dat / omk_tX.dat: byte offsets of the sub-files.
struct TitleArc {
    u32 ofs[0x10];
};
#define TITLE_ARC_PTR(arc, no) ((void*) ((arc)->ofs[no] + (u32) (arc)))

extern "C" void Title_task();

#endif
