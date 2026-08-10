#ifndef ESPGEN_H
#define ESPGEN_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "light.h"
#include "esp.h"
#include "gx.h"
#include "tpl.h"

// Optional 0x1C byte parameter block handed down the sequence calls (copied into the generator work).
struct EspSeqOpt {
    union {
        u32 x0;        // 0x00
        struct {
            u8 set;    // 0x00 bit0: speed, bit1: size, bit2: colour replace the record's (esp_sub EspSeqSet)
            u8 mul;    // 0x01 same bits: multiply
            u8 add;    // 0x02 same bits: add
            u8 x3;
        };
    };
    union {
        struct {
            u32 x4;    // 0x04
            u32 x8;    // 0x08
            u32 xC;    // 0x0C
        };
        Vec spd;       // 0x04
    };
    union {
        u32 x10;       // 0x10
        f32 sizeX;
    };
    union {
        u32 x14;       // 0x14
        f32 sizeY;
    };
    union {
        u32 x18;       // 0x18
        struct {
            u8 r;      // 0x18
            u8 g;
            u8 b;
            u8 a;
        };
    };
};

// Effect system work (game/eff_sys.cpp cEspSystem, g_pEspSys). Partial layout.
// Room effect (sst) table entry: an effect list and the offsets of its EspSeqData blocks (game/est.cpp SstSet).
struct SstList {
    u32 num;           // 0x00
    struct {
        union {
            u16 no;    // 0x00 room number range key
            struct {
                u8 x0;
                u8 id; // 0x01 display flag bit (GetSstDispFlag)
            } b;
        };
        u16 type;      // 0x02
        u32 x4;        // 0x04
    } ent[1];          // 0x04, 8 bytes each
};
struct SstData {
    u32 x0;            // 0x00
    u32 ofs[1];        // 0x04 byte offsets of the EspSeqData blocks from this header
};
struct SstTbl {
    SstData* data;     // 0x00
    SstList* list;     // 0x04
    int owner;         // 0x08 0xD2 = unused entry
};

// Area list for the room effect display flags (game/est.cpp AreaSstSet): 0x10 header, 0x98 byte entries.
struct SstAreaEnt {
    u8 x0;
    u8 x1;
    u8 bit;            // 0x02 display flag bit set while the player stands in the area
    u8 x3;
    u8 area[0x30];     // 0x04 AreaHitCheck data (AreaData)
    u32 flags;         // 0x34 bit0: the area counts as "in room" (esp_app EffAreaCheckInRoom)
    u8 pad_38[0x98 - 0x38];
};
struct SstArea {
    u32 num;           // 0x00
    u8 pad_4[0x10 - 4];
    SstAreaEnt ent[1]; // 0x10
};

// One registered effect texture set (eff_sys espTexRegist), 0x54 bytes; owner 0xD2 = free.
struct EspTexWk {
    GXTexObj* pTexObj;   // 0x00 first of nTex objects pulled from cEspSystem::texObj
    u16 nTex;            // 0x04
    u8 pad_6[2];
    GXTlutObj tlut;      // 0x08
    TEXHeader* texHdr;   // 0x14 header of texture 0
    Mtx mtx;             // 0x18
    TEXPalette* pTpl;    // 0x48
    EspAnmData* pAnm;    // 0x4C
    u32 owner;           // 0x50
};

// Effect model (efm) registration (eff_sys efmRegist), 0x14 bytes.
struct EspEfmMotTbl {
    u32 num;           // 0x00
    u32 ofs[1];        // 0x04 byte offsets of the motions from this header
};
struct EspEfmWk {
    void* model;       // 0x00 model bin
    void* tpl;         // 0x04
    EspEfmMotTbl* mot; // 0x08 motion table (NULL when none)
    void* x0C;         // 0x0C
    u32 owner;         // 0x10 0xD2 = free
};

// Effect system work (game/eff_sys.cpp, g_pEspSys, sizeof 0xC5E8).
struct cEspSystem {
    EspTexWk texWk[0x100];       // 0x0000 by texture id
    EspEfmWk efmWk[0x100];       // 0x5400 by effect model id
    SstTbl estTbl[0xD3];         // 0x6800 effect set tables by owner id
    SstTbl sstTbl[0xD3];         // 0x71E4 room effect tables by owner id
    SstTbl pathTbl[0xD3];        // 0x7BC8 path tables by owner id
    u8 ownerCnt[0xD3];           // 0x85AC EspDataLoad count per owner
    u8 pad_867F;
    SstArea* pSstArea;           // 0x8680
    GXTexObj texObj[0x1F4];      // 0x8684 texture object pool
    u8 texObjFlag[0x3F];         // 0xC504 one bit per pool entry
    u8 pad_C543[5];
    u32 xC548;         // 0xC548 number of esp slots in use
    u8* pEspBuf;       // 0xC54C esp pool (0x150 bytes per cEsp)
    u8* pEspBufSave;   // 0xC550 pool saved by EspArrayPush (esp.cpp)
    u32 xC554;         // 0xC554 number of esp slots
    u32 numSave;       // 0xC558 slot count saved by EspArrayPush
    cEsp* pDmy;        // 0xC55C dummy esp returned when the pool is full
    u8 coreKind;       // 0xC560 next effect kind handed out by EspPullCoreKind (0x45..)
    u8 toolState;      // 0xC561
    u8 pad_C562[2];
    u32 areaState;     // 0xC564 bit per area (GetAreaState)
    EspLightList lightList;  // 0xC568 lights the effects draw with (esp.cpp EspTrans -> cLightMgr::setEsp)
    u8 pad_C58C[4];
    int finalColSet;   // 0xC590 1 while finalCol.r == 0xFF (EffSetFinalCol)
    GXColor finalCol;  // 0xC594
    f32 camPan;        // 0xC598 camera yaw in degrees (EspGetCameraPan)
    f32 camPan2;       // 0xC59C camera pitch in degrees (EspGetCameraPan2)
    u32 sstDispFlag;   // 0xC5A0 room effect display flags (bit per id)
    u32 sstAddAreaFlag;  // 0xC5A4
    void (*toolCb[8])();   // 0xC5A8 tool state callbacks (state bits 0/1 set)
    void (*toolCb2[8])();  // 0xC5C8 (state bits 0/1 clear)

    int GetTexObjFlag(u32 no);
    void SetTexObjFlag(u32 no, int flag);
};
extern cEspSystem* g_pEspSys;

// One effect generator instance (game/espgen.cpp array, stride 0xC8). Bytes 0x14.. are the
// per-generator work (Espgen00Work, Espgen10Work, Espgen44Work, ...).
struct EspgenWork {
    EspInfo info;      // 0x00 owner info (copied from the parent by SetEspCore)
    u8 flag;           // 0x0C bit0: in use, bit1: delete requested
    u8 id;             // 0x0D generator id (index into the Espgen*Tbl tables)
    u8 xE;             // 0x0E
    u8 xF;             // 0x0F
    u8 step;           // 0x10 move step (Espgen*MoveTbl index)
    u8 pad_11[3];
    u8 work[0xC8 - 0x14];  // 0x14
};

// Effect controller 10 work (game/espgen10.cpp): plays an effect sequence (EspSeqData) record by
// record. est.cpp EstSet fills it directly.
struct Espgen10Work {
    EspSeqData* head;  // 0x14
    cModel* model;     // 0x18
    u32 serial;        // 0x1C model serial the controller was set up with
    u16 cnt;           // 0x20 frame counter
    u8 no;             // 0x22 next record
    u8 flags;          // 0x23 bit0: parts matrix fixed, bit1: pass the rotation on
    u16 parts;         // 0x24 parts number (0xFE: free position, 0xFF: none)
    u8 pad_26[2];
    u32 seed;          // 0x28
    Mtx mtx;           // 0x2C
    Vec pos;           // 0x5C
    Vec rot;           // 0x68
    EspSeqOpt opt;     // 0x74 copy of the option block p8 points at
    EspSeqOpt* p8;     // 0x90
};

// Water surface work shared by generators 42 (room water, game/Espgen42.cpp) and 45 (weather water,
// game/espgen45.cpp): a (nx+1) x (ny+1) height field with two ping-pong height buffers, drawn through
// a prebuilt display list with an indirect bump texture.
struct Espgen42Work {
    union {
        struct {
            u8 pad_14[4];  // 0x14
            f32 x18;       // 0x18 (espgen45: copied to xC0 by SetFreeWork)
            u8 pad_1C[4];
        };
        Vec pos0;          // 0x14 (espgen45: surface centre, SetWaterWork45)
    };
    Mtx mat;           // 0x20 grid -> world
    Mtx inv;           // 0x50 world -> grid
    u16 nx;            // 0x80 grid cells along x
    u16 ny;            // 0x82 grid cells along z
    u8 pad_84[4];
    f32 size;          // 0x88 cell size
    f32* hA;           // 0x8C height buffers (pG->flags_51E4 bit 0 selects the current one)
    f32* hB;           // 0x90
    Vec* nrm;          // 0x94
    Vec* pos;          // 0x98
    u8* dl;            // 0x9C display list
    u32 dlSize;        // 0xA0
    u8* bump;          // 0xA4 I8 bump texture (nx x ny)
    GXColor col;       // 0xA8 tev colour
    GXColor amb;       // 0xAC ambient colour (amb.a: light alpha)
    u8 mode;           // 0xB0 wave model (1: second variant)
    s8 stages;         // 0xB1 number of extra tev stages
    u8 texId;          // 0xB2
    u8 rotY;           // 0xB3 (espgen45) EspGenWork xFE
    u16 indS;          // 0xB4 indirect matrix parameters
    u16 indT;          // 0xB6
    f32 damp;          // 0xB8
    f32 spread;        // 0xBC
    f32 xC0;           // 0xC0 (espgen45)
    u8 flag;           // 0xC4 (espgen45) bit0: bounded grid (EspGenWork flags bit0), bit1: EspGenWork flags 0x4000
    u8 xC5;            // 0xC5 (espgen45) EspGenWork xC5
};

typedef void (*EspgenMoveFunc)(EspgenWork* w);
typedef void (*EspgenTransFunc)(EspgenWork* w);
typedef int (*EspgenSetFreeWorkFunc)(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts,
                                     Mtx* mtx, Vec* pos, Vec* rot, EspSeqOpt* p8, int flag);
// the application generators (Espgen4x) take no flag argument
typedef int (*EspgenSetFreeWorkAppFunc)(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model,
                                        u16 parts, Mtx* mtx, Vec* pos, Vec* rot, EspSeqOpt* p8);
typedef void (*EspgenDestructFunc)(EspgenWork* w);

extern "C" {
// game/espgen.cpp
u32 GetEspgenIdMax();
int PullEspgen(EspgenWork** out);
int PullEspgenFront(EspgenWork** out);
void PushEspgen(EspgenWork* w);
int EspgenSetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                      Vec* pos, Vec* rot, EspSeqOpt* p8, int flag);
int EspgenSeqSet(EspSeqData* head, int no, EspInfo* info, cModel* model, u16 parts, Mtx* mtx, Vec* pos, Vec* rot,
                 EspSeqOpt* p8, int flag);
void EspgenArrayClear();
void EspgenDelete(int a, int b, int c);
void EspgenDeleteEvent();
int EspgenGetCallNo();
void EspgenIncCallNo();

// game/esp_sub.cpp
int EspSeqSet(EspGenWork* rec, EspInfo* info, u32* seed, cModel* model, Mtx* mtx, int a, f32 f, cEsp** out,
              EspSeqOpt* p8, Vec* pos);

// game/est.cpp
extern cModel* EspEvModList[0x80];

// game/espgen10.cpp
int EspgenDataSet(EspSeqData* head, int no, EspInfo* info, u32* seed, cModel* model, u16 parts, Mtx* mtx, Vec* pos,
                  Vec* rot, EspSeqOpt* p8, int flag);
void SetEspCore(EspgenWork* w, int a, u32 b, u8 c, u32 d, int e);
int PullEspEspgen(EspgenWork** out, int a, int c, u32 b, u32 d, int e, int front);
void Espgen10_Move(EspgenWork* w);

// game/espgen00.cpp
void Espgen00_Move(EspgenWork* w);
int Espgen00_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* p8, int flag);

// game/espgen01.cpp
void Espgen01_Move(EspgenWork* w);
void Espgen01_Trans(EspgenWork* w);
int Espgen01_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* p8, int flag);

// game/espgen02.cpp
void Espgen02_Move(EspgenWork* w);
int Espgen02_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* p8, int flag);

// game/espgen44.cpp
void Espgen44_Move(EspgenWork* w);
void Espgen44_Trans(EspgenWork* w);
void Espgen44_Destruct(EspgenWork* w);
int Espgen44_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* p8);

// game/Espgen42.cpp
void Espgen42_Move(EspgenWork* w);
void Espgen42_Trans(EspgenWork* w);
void Espgen42_Destruct(EspgenWork* w);
int Espgen42_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* p8);

// game/Espgen43.cpp
void Espgen43_Move(EspgenWork* w);
void Espgen43_Trans(EspgenWork* w);
void Espgen43_Destruct(EspgenWork* w);
int Espgen43_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* p8);

// game/espgen45.cpp
void Espgen45_static_init();
void Estgen45SetTargetCamera(int on);
void Estgen45SetTargetHeight(int on);
void Estgen45SetSizeOverWrite(int on);
void Estgen45SetColorOverWrite(int on);
void Estgen45SetColorMul(int on);
void Estgen45SetParamOverWrite(int on);
void Estgen45SetTargetPos(f32 x, f32 z);
void Estgen45SetHeight(f32 h);
void Estgen45SetSize(f32 size);
void Estgen45SetColor(u8 r, u8 g, u8 b, u8 a, f32 rs, f32 gs, f32 bs, f32 as);
struct Esp4cWork;
void Estgen45SetParam(Esp4cWork* w);
void Espgen45_Move(EspgenWork* w);
void Espgen45_Trans(EspgenWork* w);
void Espgen45_Destruct(EspgenWork* w);
int Espgen45_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* p8);
}

// Debug tools (tools.cpp ToolArrayPush/ToolWorkPop): swap the espgen pool like EspArrayPush.
extern "C" int EspgenArrayPush(int num);
extern "C" int EspgenArrayPop();

#endif
