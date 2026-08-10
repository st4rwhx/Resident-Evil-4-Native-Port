#ifndef SCROLL_H
#define SCROLL_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Scroll (room model) data file `SMD` (game/scroll.cpp). One SmdWork per placed model.
struct SmdWork {
    Vec pos;       // 0x00
    Vec rot;       // 0x0C
    Vec scale;     // 0x18
    u8 binNo;      // 0x24  bin table index (0xFF: none)
    u8 tplNo;      // 0x25  tpl table index (0xFF: none)
    u8 motNo;      // 0x26  motion table index (0xFF: none)
    u8 id;         // 0x27  scroll object id (0xFF: unused, 0xFE: not registered)
    u8 pad_28[0x44 - 0x28];
    union {
        u32 flags;   // 0x44  bit4: bin/tpl come from the common SMD, bit6: motion too
        struct {
            u8 pad_44[3];
            u8 x47;  // 0x47  low byte of flags -> cObj::x3D0
        } b;
    };
};

class cSmd {
public:
    u8 version;    // 0x00
    u8 flags;      // 0x01  bit0: group count table in front of the works
    u16 nWork;     // 0x02
    u32 ofsBin;    // 0x04  offset table of the bins
    u32 ofsTpl;    // 0x08  offset table of the tpls
    u32 ofsMot;    // 0x0C  offset table of the motions
    union {
        SmdWork work[1];   // 0x10
        struct {
            u32 nGroup;    // 0x10
            u32 num[1];    // 0x14  works per group
        } grp;
    };

    void slide(int ofs);
    SmdWork* getWorkPtr(int no);
    void* getBinPtr(int no);
    void* getTplPtr(int no);
    void* getMotPtr(int no);
    int getWorkNum();
};

// Scroll extra data `SMX`: per-id object parameters.
struct SmxWork {
    u8 id;         // 0x00
    u8 type;       // 0x01  -> cModel::type
    u8 type2;      // 0x02  -> cModel::x12F
    u8 x3;         // 0x03  -> cModel::x135
    u32 x4;        // 0x04  -> cLightInfo::x54
    u32 flags;     // 0x08  SmxSetFlag bits
    u32 color;     // 0x0C  -> cModelInfo::color
    u8 work[0x74]; // 0x10  copied to cObj::work (0x78 bytes including color2)
    u32 color2;    // 0x84
    f32 uvScrollU; // 0x88
    f32 uvScrollV; // 0x8C
};

class cSmx {
public:
    u8 x0;         // 0x00
    u8 nWork;      // 0x01
    u8 pad_2[0x10 - 0x02];
    SmxWork work[1];   // 0x10
};

// nScrWork is not declared here on purpose: the .sbss order of scroll.cpp follows the first
// declarations (pSmd, pSmdComn, pSmx, scrObjTbl, scrTbl, nScrWork).
extern cSmd* pSmd;
extern cSmd* pSmdComn;

int SmdInit(cSmd* smd, cSmx* smx, cSmd* comn);
void SmdClear(int mode);
void workInit(cObj* obj);
void SmdSetup(int blk);
int setObj(int blk);
int SmdSetParam(cObj* obj, SmdWork* w);
void SmxSetFlag(cObj* obj, u32 flags);
int SmxGetFlag(cObj* obj);
void smxInit(cObj* obj, u8 id);
void smxInit(cObj* obj, SmxWork* w);
void* SmdGetTplPtr(int no);
cObj* SmdGetObjPtr(u32 id);
int SmdGetObjNum();
int SmdGetWorkId(cObj* obj);
void BlockCreate(int blk, cSmd* smd);
void BlockDestroy(int blk);
SmdWork* SmdGetWorkPtr(int id);
cObj* SmdGetGroupObjPtr(u32 id);
cObj* SmdGetGroupObjPtr2(u32 id);
cObj* SmdGetGroupNext(cObj* obj);
void SmdSetTrans(u32 id, int on);
cObj* SetObjSmd(void* bin, void* tpl, Vec* pos, Vec* rot, int lightFlag, int front);

#endif
