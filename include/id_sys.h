#ifndef ID_SYS_H
#define ID_SYS_H

#include "types.h"
#include "vec.h"
#include "hermite.h"

// Screen id (widget) unit (game/id_sys.cpp), 0x138 bytes.
struct IdUnit {
    u8 flags;        // 0x00  0xFF: free; 0x01: alive, 0x02: just set, 0x04: move, 0x08: visible, 0x10: drawing
    u8 unitNo;       // 0x01  own number (parent lookup key)
    u8 type;         // 0x02  id table type (IDSystem::set parameter)
    u8 id;           // 0x03  unit id inside the table
    u8 kind;         // 0x04  1: group (children follow)
    u8 level;        // 0x05  depth in the parent tree
    u8 parentNo;     // 0x06
    u8 x7;           // 0x07
    Mtx mat;         // 0x08  world matrix
    Mtx localMat;    // 0x38
    IdUnit* parent;  // 0x68
    u8 texId;        // 0x6C
    u8 maskId;       // 0x6D
    u8 no;           // 0x6E  texture frame (stage: digit)
    u8 maskNo;       // 0x6F  mask texture frame
    u8 texCnt;       // 0x70
    u8 maskCnt;      // 0x71
    u16 timer[4];    // 0x72  (converted as s16)  path / scale / color / rotation curve times
    u8 vtxType;      // 0x7A  low nibble: anchor (IdCalcVertex)
    u8 loop;         // 0x7B  bit n: timer n loops
    u8 scaleType;    // 0x7C  0x10: scale x only, 0x20: y only
    u8 rotAxis;      // 0x7D
    u8 dir;          // 0x7E  bit n: timer n counts up
    u8 flags_7F;     // 0x7F  0x01: mask texture, 0x02: no texture animation, 0x04: no mask animation
    u8 ot;           // 0x80
    u8 prio;         // 0x81
    u8 transType;    // 0x82  0: common, 1: negative, 2/3: shimmer
    u8 transSub;     // 0x83
    u8 blendType;    // 0x84
    u8 end;          // 0x85  bit n: timer n finished
    u8 pad_86[2];
    Vec scr;         // 0x88  screen position
    Vec pos;         // 0x94  path offset + scr (world position used for drawing)
    Vec vtx[4];      // 0xA0
    f32 sizeX;       // 0xD0
    f32 sizeY;       // 0xD4
    u8 pad_D8[8];
    u8 col0[4];      // 0xE0
    u8 col1[4];      // 0xE4
    f32 col[4];      // 0xE8
    Vec rot;         // 0xF8
    Vec rotCur;      // 0x104
    f32 u0;          // 0x110
    f32 u1;          // 0x114
    f32 v0;          // 0x118
    f32 v1;          // 0x11C
    void* path0;     // 0x120  FuncPath data
    void* path1;     // 0x124
    Hermite1* curve[4];  // 0x128
};

// One entry of an id data table (IDSystem::set), version 1 = 0x88 bytes, version 2 = 0x8C bytes.
struct IdData {
    u8 pad_0[3];
    u8 flags;        // 0x03
    u8 id;           // 0x04
    u8 no;           // 0x05
    u8 level;        // 0x06
    u8 parentNo;     // 0x07
    u8 x8;           // 0x08
    u8 kind;         // 0x09
    u8 pad_A;
    u8 texId;        // 0x0B
    u8 vtxType;      // 0x0C
    u8 loop;         // 0x0D
    u8 scaleType;    // 0x0E
    u8 rotAxis;      // 0x0F
    u8 dir;          // 0x10
    u8 pad_11[3];
    Vec pos;         // 0x14
    Vec vtx[4];      // 0x20
    f32 sizeX;       // 0x50
    f32 sizeY;       // 0x54
    u8 col0[4];      // 0x58
    // version 1
    Vec rot;         // 0x5C
    u8 blendType;    // 0x68
    u8 transType;    // 0x69
    u8 maskId;       // 0x6A
    u8 flags_7F;     // 0x6B
    u8 transSub;     // 0x6C
    u8 pad_6D[3];
    u32 ofs[6];      // 0x70  path0, path1, curve[4] (offsets from the table start, 0 = none)
};

struct IdData2 {
    u8 pad_0[3];
    u8 flags;        // 0x03
    u8 id;           // 0x04
    u8 no;           // 0x05
    u8 level;        // 0x06
    u8 parentNo;     // 0x07
    u8 x8;           // 0x08
    u8 kind;         // 0x09
    u8 pad_A;
    u8 texId;        // 0x0B
    u8 vtxType;      // 0x0C
    u8 loop;         // 0x0D
    u8 scaleType;    // 0x0E
    u8 rotAxis;      // 0x0F
    u8 dir;          // 0x10
    u8 pad_11[3];
    Vec pos;         // 0x14
    Vec vtx[4];      // 0x20
    f32 sizeX;       // 0x50
    f32 sizeY;       // 0x54
    u8 col0[4];      // 0x58
    u8 col1[4];      // 0x5C
    Vec rot;         // 0x60
    u8 blendType;    // 0x6C
    u8 transType;    // 0x6D
    u8 maskId;       // 0x6E
    u8 flags_7F;     // 0x6F
    u8 transSub;     // 0x70
    u8 pad_71[3];
    u32 ofs[6];      // 0x74
};

// Id data table header: version string, entry count, entries from 0x08.
struct IdDataHeader {
    char version[5];  // 0x00  "1.00" / "2.00"
    u8 num;           // 0x05
    u8 pad_6[2];
};

class IDSystem {
public:
    s32 num;          // 0x00
    s32 active;       // 0x04
    s32 maxLevel;     // 0x08
    u32 ck[8];        // 0x0C  table types set
    u32 disp[8];      // 0x2C  table types hidden
    IdUnit* pUnit;    // 0x4C

    static Mtx m_scrn_mat;

    void gameInit(int n);
    void roomInit();
    void free();
    int setCk(u8 type);
    void dispSw(u8 type, int sw);
    void unitPush(IdUnit* u);
    IdUnit* unitPull();
    void unitLevel(IdUnit* u, u8 level);
    void unitParent(IdUnit* parent, IdUnit* child);
    IdUnit* unitPtr(u8 id, u8 type);
    // card.cpp passes `0x40 + i` without the `clrlwi` truncation: int view of the type parameter.
    IdUnit* unitPtrI(u8 id, int type) asm("unitPtr__8IDSystemUcUc");
    void set(void* data, u8 id, u8 type, u8 ot, u8 prio, u8 mode);
    // card.cpp passes `0x40 + i` without the `clrlwi` truncation: int view of the type parameter.
    void setI(void* data, u8 id, int type, u8 ot, u8 prio, u8 mode) asm("set__8IDSystemPvUcUcUcUcUc");
    void kill(u8 id, u8 type);
    // COMPILER-DIFF: item 4 (narrow-argument truncation). Sscrn IdNumErase passes `0x40 + i` without
    // the `clrlwi`: int view of the type parameter.
    void killI(u8 id, int type) asm("kill__8IDSystemUcUc");
    void stop();
    void move();
    void beMove(IdUnit* u, int sw);
    void setTime(IdUnit* u, u16 time);
    // option.cpp passes a sign-extended value (`extsh` before the call): the declaration it was
    // compiled against took a signed short. Same function, signed view of the parameter.
    void setTimeS(IdUnit* u, s16 time) asm("setTime__8IDSystemP6IdUnitUs");
    void movePos(IdUnit* u);
    void trans();
    void unitTrans(IdUnit* u);
};

extern IDSystem IdSys;
extern void* g_pIdBuff;
extern int IdBuffType;

// game/id_tex.cpp
struct TexWk;
struct TexAnm;
void IdTexSet(u8 id, u8 no);
int IdGetAnmAddr(u8 id, TexAnm** out);
void IdChannelSet(IdUnit* u);
TexWk* IdGetTexWk(u8 id, int quiet);

extern "C" {
void idSysMove00(IdUnit* u);
void IdCalcVertex(IdUnit* u);
void idSysMove01(IdUnit* u);
void idSysMove02(IdUnit* u);
void idSysMove03(IdUnit* u);
void idSysMove04(IdUnit* u);
void IdGeneralTrans(IdUnit* u);
void IdCommonTrans(IdUnit* u);
void IdNegativeTrans(IdUnit* u, u32 mode);
void IdShimmerTrans(IdUnit* u, int sub, int type);
void IdAllocBuffer();
void IdFreeBuffer();
void IdDebugAllocBuffer();
void IdDebugFreeBuffer();
void* IdGetBufferAddr(int type);
void IdSetBufferType(int type);
void IdTexGameInit();
void IdTexRoomInit();
void IdTexRelease(int id);
int IdTexDataLoad(void* data, int id);
}

#endif
