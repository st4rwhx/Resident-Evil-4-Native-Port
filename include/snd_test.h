#ifndef SND_TEST_H
#define SND_TEST_H

#include "types.h"
// the driver header declares the work under its own type; this header owns the tool's view
#ifndef SND_SDK_NO_GX
#define SND_SDK_NO_GX
#endif
#define Snd_test_work Snd_test_work_drv_view
#include "snd_drv.h"
#undef Snd_test_work

// Sound test of the t_movie REL (t_movie/snd_test.cpp: Snd_test_*/test_*/disp_*/aram_*; the entry
// SoundTest and the file-list helper live in t_movie.cpp). The work is the sound driver's
// Snd_test_work (snd_ram.c, 0x7C0 bytes; snd_drv.h's SND_TEST_WORK is the driver-side view).
struct SndTestWork {
    s8 mode;      // 0x00  menu cursor / test mode (0 SIT, 1 RIT, 2 AUX A, 3 AUX B, 4 VOL, 5 DUMP, 6/7 LOAD)
    s8 tbl;       // 0x01  0 = SIT (ISS), 1 = RIT (stream)
    s8 type;      // 0x02  SIT type (0 dummy, 1 normal, 2 ADSR, 3 MIDI)
    s8 aux;       // 0x03  effect slot being edited (0 AUX A, 1 AUX B)
    s8 loadTbl;   // 0x04  load menu table (0 SIT, 1 RIT)
    u8 wtDisp;    // 0x05  show the wavetable data instead of the SIT
    u8 seqDisp;   // 0x06  show the sequencer channels instead of the MIDI SIT
    u8 pad_7;
    u32 frame;    // 0x08
    s16 menu;     // 0x0C  1 = mode menu shown
    u16 dispFlag; // 0x0E  0x1 request parameters, 0x2 voice map, 0x4 aux state
    u32 sndId;    // 0x10  last issued sound
    s16 blkNo[2]; // 0x14  current block per table
    s16 blkMax[2];    // 0x18
    s16 reqNo[2]; // 0x1C  current request per table
    s16 reqCur;   // 0x20  request being edited
    s16 reqMax[2];    // 0x22
    s8 cursor[2]; // 0x26  parameter cursor per table
    s8 cur;       // 0x28  parameter cursor being edited
    u8 pad_29;
    s16 efxType[2];   // 0x2A  effect type per slot
    s16 efxCur;   // 0x2E
    s8 efxState[2];   // 0x30  1 executed, 0 changing, -1 stopped
    s8 auxCursor[2];  // 0x32
    s8 auxCur;    // 0x34
    s8 volCursor; // 0x35
    u8 pad_36[2];
    u32 on;       // 0x38  Joy[0] copy taken by SoundTest every frame
    u32 old;      // 0x3C
    u32 trg;      // 0x40
    u32 rep;      // 0x44
    SND_SIT sit;  // 0x48  SIT being edited
    SND_RIT rit;  // 0x60  RIT being edited
    SND_SIT* pSit;    // 0x70
    SND_RIT* pRit;    // 0x74
    DVDDir dir;   // 0x78
    DVDDirEntry ent;  // 0x84
    int dirNum;   // 0x90
    int dirTop;   // 0x94
    int dirCur;   // 0x98
    char path[2][0x100];  // 0x9C
    char* dirName[0x80];  // 0x29C
    u8 dirIsDir[0x80];    // 0x49C
    u32 sitData[14];  // 0x51C
    u32 ritData[2];   // 0x554
    u8 pad_55C[0x69C - 0x55C];
    u8* wt;       // 0x69C
    WTINST* inst; // 0x6A0
    WTREGION* rgn;    // 0x6A4
    WTART* art;   // 0x6A8
    WTSAMPLE* sample; // 0x6AC
    WTADPCM* adpcm;   // 0x6B0
    SND_AXV_WORK* axv;    // 0x6B4
    u32 aramAdrs; // 0x6B8
    u8 pad_6BC[4];
    u8 dump[0x100];   // 0x6C0  ARAM dump buffer
};

extern SndTestWork Snd_test_work;

int Snd_test_mode();
const char* Snd_test_get_str_name(int type);

#endif
